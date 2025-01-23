#pragma once
#include "../price.hpp"
#include "order.hpp"
#include <cstdint>
#include <variant>
#include <vector>
namespace defi {
class Pool_uint64;
class Matcher;

struct BaseQuote_uint64;
struct Delta_uint64 {
  bool isQuote{false};
  uint64_t amount;
  BaseQuote_uint64 base_quote() const;
};
struct BaseQuote_uint64 {
  uint64_t base;
  uint64_t quote;
  BaseQuote_uint64 operator-(const Delta_uint64 &bq) const {
    auto res{*this};
    if (bq.isQuote)
      res.quote -= bq.amount;
    else
      res.base -= bq.amount;
    return res;
  }
  auto price() const { return PriceRelative::from_fraction(quote, base); }
};
inline BaseQuote_uint64 Delta_uint64::base_quote() const {
  if (isQuote)
    return {0, amount};
  return {amount, 0};
}

struct FillResult_uint64 {
  std::optional<Delta_uint64> toPool;
  std::optional<Delta_uint64> notFilled;
  BaseQuote_uint64 filled;
};
struct MatchResult_uint64 : public FillResult_uint64 {
  size_t baseBound;
  size_t quoteBound;
};

class Evaluator {
  static std::pair<std::strong_ordering, Prod192> take_smaller(Prod192 &a,
                                                               Prod192 &b) {
    auto rel{a <=> b};
    if (rel == std::strong_ordering::less)
      return {rel, a};
    return {rel, b};
  }

public:
  Evaluator(uint64_t basePool, uint64_t baseIn, uint64_t quotePool,
            uint64_t quoteIn)
      : in{baseIn, quoteIn}, pool{basePool, quotePool} {}

  BaseQuote_uint64 in;
  BaseQuote_uint64 pool;
  struct ret_t {
    std::strong_ordering rel;
    struct Pool128Ratio {
      Prod128 a, b;
    };
    struct Fill64Ratio {
      uint64_t a, b;
    };
    bool exceeded() const { return std::holds_alternative<Pool128Ratio>(v); }
    auto &get_unexceeded_ratio() const { return std::get<Fill64Ratio>(v); };
    auto &get_exceeded_ratio() const { return std::get<Pool128Ratio>(v); };
    std::variant<Pool128Ratio, Fill64Ratio> v;
  };

  [[nodiscard]] ret_t rel_base_asc(uint64_t baseDelta) const {
    auto pp1{Prod128(pool.base, pool.quote)};
    auto pp2{in.base - baseDelta};
    auto pp{pp1 * pp2};
    auto op1{Prod128(pool.base + baseDelta, pool.base + baseDelta)};
    auto op2{in.quote};
    auto op{op1 * op2};

    auto rel{op <=> pp};
    if (rel == std::strong_ordering::greater) {
      return {rel, ret_t::Pool128Ratio{pp1, op1}};
    }
    return {rel, ret_t::Fill64Ratio{pp2, op2}};
  }
  [[nodiscard]] ret_t rel_quote_asc(uint64_t quoteToPool) const {
    auto pp1{Prod128(pool.quote, pool.base)};
    auto pp2{in.quote - quoteToPool};
    auto pp{pp1 * pp2};
    auto op1{Prod128(pool.quote + quoteToPool, pool.quote + quoteToPool)};
    auto op2{in.base};
    auto op{op1 * op2};
    auto rel{op <=> pp};
    if (rel == std::strong_ordering::greater) {
      return {rel, ret_t::Pool128Ratio{pp1, op1}};
    }
    return {rel, ret_t::Fill64Ratio{pp2, op2}};
  }

  [[nodiscard]] std::strong_ordering rel_quote_price(uint64_t quoteToPool,
                                                     Price p) const {
    return compare_fraction(
        Prod128(pool.quote + quoteToPool, pool.quote + quoteToPool),
        Prod128(pool.base, pool.quote), p);
  }
  [[nodiscard]] std::strong_ordering rel_base_price(uint64_t baseToPool,
                                                    Price p) const {
    return compare_fraction(
        Prod128(pool.base, pool.quote),
        Prod128(pool.base + baseToPool, pool.base + baseToPool), p);
  }
  [[nodiscard]] Prod192 quotedelta_quadratic_asc(uint64_t quoteDelta) const {
    return Prod128(pool.quote + quoteDelta, pool.quote + quoteDelta) * in.base;
  }
};

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
