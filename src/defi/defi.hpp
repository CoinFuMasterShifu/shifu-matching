#pragma once
#include "../funds.hpp"
#include "defi_uint64/matching.hpp"
#include "defi_uint64/pool.hpp"
namespace defi {

struct Order {
  Funds amount;
  Price limit;
};

class Pool {
  friend class BuySellOrders;
  struct BuyTx {
    Funds sellQuote;
  };
  struct BuyResult {
    Funds payQuote;
    Funds receiveBase;
  };

  struct SellTx {
    Funds sellBase;
  };
  struct SellResult {
    Funds payBase;
    Funds receiveQuote;
  };

public:
  Pool(Funds base, Funds quote) : pool_uin64(base.E8(), quote.E8()) {}
  Funds base_total() const {
    return Funds::from_value_throw(pool_uin64.base_total());
  }
  Funds quote_total() const {
    return Funds::from_value_throw(pool_uin64.quote_total());
  }

  [[nodiscard]] Funds sell(Funds baseAdd, uint64_t feeE4 = 10) {
    return Funds::from_value_throw(pool_uin64.sell(baseAdd.E8(), feeE4));
  }
  [[nodiscard]] Funds buy(Funds quoteAdd, uint64_t feeE4 = 10) {
    return Funds::from_value_throw(pool_uin64.buy(quoteAdd.E8(), feeE4));
  }

  BuyResult apply(BuyTx tx) {
    return {.payQuote = tx.sellQuote,
            .receiveBase =
                Funds::from_value_throw(pool_uin64.buy(tx.sellQuote.E8()))};
  }
  SellResult apply(SellTx tx) {
    return {.payBase = tx.sellBase,
            .receiveQuote =
                Funds::from_value_throw(pool_uin64.sell(tx.sellBase.E8()))};
  }

private:
  Pool_uint64 pool_uin64;
};

struct BaseQuote;
struct Delta {
  Delta(Delta_uint64 d) : d(std::move(d)) {}
  Funds amount() const { return Funds::from_value_throw(d.amount); }
  bool is_quote() const { return d.isQuote; }
  bool is_base() const { return !is_quote(); }
  BaseQuote base_quote() const;

private:
  Delta_uint64 d;
};

struct BaseQuote {
  BaseQuote(BaseQuote_uint64 bq)
      : base(Funds::from_value_throw(bq.base)),
        quote(Funds::from_value_throw(bq.quote)) {}
  BaseQuote(Funds base, Funds quote) : base(base), quote(quote) {}
  Funds base;
  Funds quote;
  [[nodiscard]] BaseQuote diff_throw(const BaseQuote &bq) const {
    return {Funds::diff_throw(base, bq.base),
            Funds::diff_throw(quote, bq.quote)};
  }
  [[nodiscard]] auto diff_throw(const Delta &d) const {
    return diff_throw(d.base_quote());
  }
  auto price() const {
    return PriceRelative::from_fraction(quote.E8(), base.E8());
  }
};
inline BaseQuote Delta::base_quote() const {
  if (is_quote())
    return {Funds::zero(), amount()};
  return {amount(), Funds::zero()};
}
class BuySellOrders {
  class OrderView {
  public:
    Funds amount() const {
      return Funds::from_value_throw(order_uint64.amount);
    }
    auto limit() const { return order_uint64.limit; }
    OrderView(const Order_uint64 &o) : order_uint64(o) {};

  private:
    const Order_uint64 &order_uint64;
  };

  class OrderVecView {
  public:
    OrderVecView(const ordervec::Ordervec &ov) : ov(ov) {}
    OrderView operator[](size_t i) const { return ov[i]; }
    auto size() const { return ov.size(); }

  private:
    const ordervec::Ordervec &ov;
  };

  class MatchResult {
  public:
    MatchResult(MatchResult_uint64 mr) : mr(std::move(mr)) {}
    Delta to_pool() const { return mr.toPool; }
    std::optional<Delta> not_filled() const {
      if (mr.notFilled)
        return Delta(*mr.notFilled);
      return {};
    }
    BaseQuote filled() const { return mr.filled; }
    size_t quote_bound() { return mr.quoteBound; }
    size_t base_bound() { return mr.baseBound; }

  private:
    MatchResult_uint64 mr;
  };

public:
  [[nodiscard]] MatchResult match(Pool &p) {
    return buySellOrders_uint64.match(p.pool_uin64);
  }
  auto insert_base(Order o) {
    return buySellOrders_uint64.insert_base(
        {.amount = o.amount.E8(), .limit{o.limit}});
  }
  auto insert_quote(Order o) {
    return buySellOrders_uint64.insert_quote(
        {.amount = o.amount.E8(), .limit{o.limit}});
  }
  OrderVecView quote_desc_buy() const {
    return buySellOrders_uint64.quote_desc_buy();
  }
  OrderVecView base_asc_sell() const {
    return buySellOrders_uint64.base_asc_sell();
  }

private:
  BuySellOrders_uint64 buySellOrders_uint64;
};

} // namespace defi
