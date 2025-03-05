#pragma once

#include "matcher.hpp"
#include "matcher.hpp"
#include "orderbook.hpp"
#include "pool.hpp"

namespace defi {

[[nodiscard]] inline MatchResult_uint64 match_lazy(auto loadSellAsc, auto loadBuyDesc, const PoolLiquidity_uint64& pool)
{
    assert(pool.price()); // TODO: ensure the pool is non-degenerate
    const auto price { *pool.price() };

    Orderbook_uint64 ob;
    std::optional<Price_uint64> lower, upper;
    BaseQuote_uint64 filled { 0, 0 };

    // load sell orders below pool price
    std::optional<Order_uint64> sellOrderUpper;
    size_t I { 0 }; // sell index bound
    while (true) {
        std::optional<Order_uint64> o { loadSellAsc() };
        if (!o)
            break;
        ob.insert_largest_base(*o);
        if (o->limit < price) {
            lower = o->limit;
            filled.base += o->amount.value();
            I = ob.base_asc_sell().size();
        } else {
            upper = o->limit;
            sellOrderUpper = *o;
            break;
        }
    }

    // load buy orders above pool price
    std::optional<Order_uint64> buyOrderLower;
    size_t J { 0 }; // buy index bound
    while (true) {
        std::optional<Order_uint64> o { loadBuyDesc() };
        if (!o)
            break;
        ob.insert_smallest_quote(*o);
        if (o->limit > price) {
            if (!upper || *upper > o->limit)
                upper = o->limit;
            filled.quote += o->amount.value();
            J = ob.quote_desc_buy().size();
        } else {
            if (!lower || *lower < o->limit)
                lower = o->limit;
            buyOrderLower = *o;
            break;
        }
    }

    auto more_quote_less_base = [&](Price_uint64 p) {
        Delta_uint64 toPool { filled.excess(p) };
        return !pool.modified_pool_price_exceeds(toPool, p);
    };

    auto upper_buy_bound { [&]() -> const Order_uint64* {
        if (J == 0)
            return nullptr;
        return &ob.quote_desc_buy()[(J - 1)];
    } };

    auto lower_sell_bound { [&]() -> const Order_uint64* {
        if (I == 0)
            return nullptr;
        return &ob.base_asc_sell()[(I - 1)];
    } };

    auto shift_buy_higher { [&]() {
        assert(J != 0);
        filled.quote -= upper_buy_bound()->amount.value();
        J -= 1;
    } };

    auto shift_sell_smaller { [&]() {
        assert(I != 0);
        filled.base -= upper_buy_bound()->amount.value();
        I -= 1;
    } };

    if (upper && !more_quote_less_base(*upper)) {
        auto nextSell { sellOrderUpper };
        while (nextSell) {
            ob.insert_largest_base(*nextSell);
            while (true) {
                auto usb { upper_buy_bound() };
                if (usb && usb->limit < nextSell->limit)
                    shift_buy_higher();
                else
                    break;
            }
            filled.base += nextSell->amount.value();
            if (more_quote_less_base(nextSell->limit))
                break;
            nextSell = loadSellAsc();
        }
    } else if (lower && more_quote_less_base(*lower)) {
        auto nextBuy { buyOrderLower };
        while (nextBuy) {
            ob.insert_smallest_quote(*nextBuy);
            while (true) {
                auto lsb { lower_sell_bound() };
                if (lsb && lsb->limit > nextBuy->limit)
                    shift_sell_smaller();
                else
                    break;
            }
            filled.quote += nextBuy->amount.value();
            if (!more_quote_less_base(nextBuy->limit))
                break;
            nextBuy = loadBuyDesc();
        }
    }
    return ob.match(pool);
}
}
