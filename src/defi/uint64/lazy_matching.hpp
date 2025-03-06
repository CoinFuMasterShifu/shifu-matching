#pragma once

#include "matcher.hpp"
#include "orderbook.hpp"
#include "pool.hpp"
#include <iostream>
using namespace std;

namespace defi {

[[nodiscard]] inline MatchResult_uint64 match_lazy(auto& loaderSellAsc, auto& loaderBuyDesc, const PoolLiquidity_uint64& pool)
{
    assert(pool.price()); // TODO: ensure the pool is non-degenerate
    const auto price { *pool.price() };

    Orderbook_uint64 ob;
    std::optional<Price_uint64> lower, upper;
    BaseQuote_uint64 filled { 0, 0 };

    // load sell orders below pool price
    size_t I { 0 }; // sell index bound
    std::optional<Price_uint64> p;
    while (true) {
        cout << "S0 -----------------------\n";
        auto np { loaderSellAsc.next_price() };
        if (!np)
            break;
        assert(!p || *p < *np); // prices must be strictly increasing
        p = np;
        if (*p <= price) {
            Order_uint64 o { loaderSellAsc.load_next_order() };
            ob.insert_largest_base(o);
            cout << "S1 " << o.limit.to_double() << "-----------------------\n";
            lower = o.limit;
            filled.base.add_assert(o.amount.value());
            I = ob.base_asc_sell().size();
        } else {
            cout << "S2 -----------------------\n";
            upper = *p;
            break;
        }
    }

    // load buy orders above pool price
    size_t J { 0 }; // buy index bound
    p.reset();
    while (true) {
        cout << "B0 -----------------------\n";
        auto np { loaderBuyDesc.next_price() };
        if (!np)
            break;
        cout << "B0 "<<np->to_double()<<endl;
        assert(!p || *p > *np); // prices must be strictly decreasing
        p = np;
        if (*p > price) { // now require strictness to avoid selecting degenerate (zero-length) section
            Order_uint64 o { loaderBuyDesc.load_next_order() };
            ob.insert_smallest_quote(o);
            cout << "B1 " << o.limit.to_double() << "-----------------------\n";
            if (!upper || *upper > o.limit)
                upper = o.limit;
            filled.quote.add_assert(o.amount.value());
            J = ob.quote_desc_buy().size();
        } else {
            if (!lower || *lower < *p)
                lower = *p;
            cout << "B2-----------------------\n";
            cout << "*p: "<<p->to_double()<<endl;
            cout << "lower: "<<lower->to_double()<<endl;
            break;
        }
    }

    assert(!upper || upper != lower); // we cannot have degenerate (zero-length) section

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
        cout << "I = " << I << endl;
        return &ob.base_asc_sell()[I - 1];
    } };

    auto shift_buy_higher { [&]() {
        assert(J != 0);
        filled.quote.subtract_assert(upper_buy_bound()->amount.value());
        J -= 1;
    } };

    auto shift_sell_smaller { [&]() {
        assert(I != 0);
        filled.base.subtract_assert(lower_sell_bound()->amount.value());
        I -= 1;
    } };

    cout << "Shifts------------------------\n";
    cout << " lower: ";
    if (lower) {
        cout << lower->to_double();
    }
    cout<< endl;
    cout << " upper: ";
    if (upper) {
        cout << upper->to_double();
    }
    cout<< endl;
    if (upper && !more_quote_less_base(*upper)) {
        cout << "S -----------------------\n";
        std::optional<Price_uint64> np { loaderSellAsc.next_price() };
        while (np) {
            cout << "S1 " << np->to_double() << "-----------------------\n";

            while (auto b { upper_buy_bound() }) {
                if (b->limit < *np)
                    shift_buy_higher();
                else
                    break;
            }

            cout << "S3 -----------------------\n";
            if (more_quote_less_base(*np))
                break;
            cout << "S4 -----------------------\n";
            Order_uint64 o { loaderSellAsc.load_next_order() };
            assert(*np == o.limit);
            ob.insert_largest_base(o);
            filled.base.add_assert(o.amount.value());
            np = loaderSellAsc.next_price();
        }
    } else if (lower && more_quote_less_base(*lower)) {
        cout << "B -----------------------\n";
        std::optional<Price_uint64> np { loaderBuyDesc.next_price() };
        while (np) {
            cout << "B1 " << np->to_double() << "-----------------------\n";

            while (auto b { lower_sell_bound() }) {
                if (b->limit > *np)
                    shift_sell_smaller();
                else
                    break;
            }

            cout << "B3 -----------------------\n";

            if (!more_quote_less_base(*np))
                break;
            cout << "B4 -----------------------\n";
            Order_uint64 o { loaderBuyDesc.load_next_order() };
            assert(*np == o.limit);
            ob.insert_smallest_quote(o);
            filled.quote.add_assert(o.amount.value());
            np = loaderBuyDesc.next_price();
        }
    }
    cout << "D -----------------------\n";

    using namespace std;
    cout << "lazy_match order book loaded" << endl;
    return ob.match(pool);
}
}
