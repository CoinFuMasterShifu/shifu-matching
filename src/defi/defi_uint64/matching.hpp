#pragma once
#include "../price.hpp"
#include "matcher.hpp"
#include "order.hpp"
#include <cstdint>
#include <vector>
namespace defi {
class Pool_uint64;
class Matcher;




namespace ordervec {
struct elem_t : public Order_uint64 {
  elem_t(Order_uint64 o) : Order_uint64(std::move(o)) {}

  auto operator<=>(const elem_t &o) const {
    return limit.operator<=>(o.limit);
  };
  auto operator==(const elem_t &o) const { return limit == o.limit; }
};

struct Ordervec : protected std::vector<elem_t> {
  using parent_t = std::vector<elem_t>;
  Ordervec() {}
  using parent_t::erase;
  using parent_t::size;
  using parent_t::operator[];
  bool delete_at(size_t i) {
    if (i < size()) {
      erase(begin() + i);
      return true;
    }
    return false;
  }
  void insert_desc(ordervec::elem_t o) {
    auto iter{std::lower_bound(rbegin(), rend(), o)};
    if (iter != rend() && *iter == o)
      iter->amount += o.amount;
    else
      insert(iter.base(), std::move(o));
    total += o.amount;
  }
  void insert_asc(ordervec::elem_t o) {
    auto iter{std::lower_bound(begin(), end(), o)};
    if (iter != end() && *iter == o)
      iter->amount += o.amount;
    else
      insert(iter, std::move(o));
    total += o.amount;
  }
  auto total_push() { return total; }

private:
  uint64_t total{0};
};

class BuySellOrders_uint64 {
  friend class defi::Matcher;
  struct ExtraData {
    uint64_t cumsum;
    size_t upperBoundCounterpart;
  };

  void prepare() {
    extraBase.resize(0);
    extraQuote.resize(0);
    uint64_t cumsumQuote{0};
    const size_t J{pushBaseAsc.size()};
    const size_t I{pushQuoteDesc.size()};
    uint64_t cumsumBase{pushBaseAsc.total_push()};
    size_t j{0};
    extraBase.resize(0);
    for (size_t i = 0; i < I; ++i) {
      auto &oq{pushQuoteDesc[i]};
      for (; j < J; ++j) {
        auto &ob{pushBaseAsc[J - 1 - j]};
        if (ob.limit <= oq.limit)
          break;
        extraBase.push_back({cumsumBase, i});
        cumsumBase -= ob.amount;
      }
      extraQuote.push_back({cumsumQuote, j});
      cumsumQuote += oq.amount;
    }
    for (; j < J; ++j) {
      extraBase.push_back({cumsumBase, I});
      cumsumBase -= pushBaseAsc[J - 1 - j].amount;
    }
  }

public:
  [[nodiscard]] MatchResult_uint64 match(Pool_uint64 &p);
  [[nodiscard]] MatchResult_uint64 match_internal(Pool_uint64 &p);
  auto insert_base(Order_uint64 o) {
    pushBaseAsc.insert_asc(elem_t{o});
    prepare();
  }
  auto insert_quote(Order_uint64 o) {
    pushQuoteDesc.insert_desc(elem_t{o});
    prepare();
  }
  bool delete_quote(size_t i) {
    if (pushQuoteDesc.delete_at(i)) {
      prepare();
      return true;
    }
    return false;
  }

  bool delete_base(size_t i) {
    if (pushBaseAsc.delete_at(pushBaseAsc.size() - 1 - i)) {
      prepare();
      return true;
    }
    return false;
  }
  auto &quote_desc_buy() const { return pushQuoteDesc; }
  auto &base_asc_sell() const { return pushBaseAsc; }

private:
  Ordervec pushQuoteDesc; // limit price DESC (buy)
  std::vector<ExtraData> extraQuote;

  Ordervec pushBaseAsc; // limit price ASC (sell)
  std::vector<ExtraData> extraBase;
};
} // namespace ordervec
using ordervec::BuySellOrders_uint64;

} // namespace defi
