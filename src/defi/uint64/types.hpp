#pragma once
#include "general/funds.hpp"
#include "price.hpp"
#include <cstdint>

namespace defi {
struct Order_uint64 {
    Funds_uint64 amount;
    Price_uint64 limit;
};

struct BaseQuote_uint64;
struct Delta_uint64 {
    bool operator==(const Delta_uint64&) const = default;
    bool isQuote { false };
    Funds_uint64 amount;
    BaseQuote_uint64 base_quote() const;
};

struct BaseQuote_uint64 {
    Funds_uint64 base;
    Funds_uint64 quote;
    bool operator==(const BaseQuote_uint64&) const = default;
    BaseQuote_uint64(Funds_uint64 base, Funds_uint64 quote)
        : base(base)
        , quote(quote)
    {
    }
    // BaseQuote_uint64 operator-(const Delta_uint64& bq) const
    // {
    //     auto res { *this };
    //     if (bq.isQuote)
    //         res.quote.subtract_assert(bq.amount);
    //     else
    //         res.base -= bq.amount;
    //     return res;
    // }
    Delta_uint64 excess(Price_uint64 p) const // computes excess
    {
        auto q { multiply_floor(base.value(), p) };
        if (q.has_value() && *q <= quote) // too much base
            return { true, Funds_uint64::diff_assert(quote, *q) };
        auto b { divide_floor(quote.value(), p) };
        assert(b.has_value()); // TODO: verify assert by checking precision of
                               // divide_floor
        assert(*b <= base);
        return { false, Funds_uint64::diff_assert(base, *b) };
    }
    auto price() const { return PriceRelative_uint64::from_fraction(quote.value(), base.value()); }
};

inline BaseQuote_uint64 Delta_uint64::base_quote() const
{
    if (isQuote)
        return { 0, amount };
    return { amount, 0 };
}
}
