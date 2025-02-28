#pragma once
#include "src/defi/defi_uint64/pool.hpp"
#include "src/defi/defi_uint64/prod.hpp"
#include "src/defi/price.hpp"
#include <utility>
#include <variant>

namespace defi {
struct BaseQuote_uint64;
struct Delta_uint64 {
    bool isQuote { false };
    uint64_t amount;
    BaseQuote_uint64 base_quote() const;
};


struct BaseQuote_uint64 {
    uint64_t base;
    uint64_t quote;
    BaseQuote_uint64 operator-(const Delta_uint64& bq) const
    {
        auto res { *this };
        if (bq.isQuote)
            res.quote -= bq.amount;
        else
            res.base -= bq.amount;
        return res;
    }
    Delta_uint64 excess(Price p) const // computes excess
    {
        auto q { multiply_floor(base, p) };
        if (q.has_value() && *q <= quote) // too much base
            return { true, quote - *q };
        auto b { divide_floor(quote, p) };
        assert(b.has_value()); // TODO: verify assert by checking precision of
                               // divide_floor
        assert(*b <= base);
        return { false, base - *b };
    }
    auto price() const { return PriceRelative::from_fraction(quote, base); }
};

struct PoolLiquidity_uint64 : public BaseQuote_uint64 {

    Ratio128 price_ratio_added_quote(uint64_t quoteToPool) const
    {
        return {
            .numerator { Prod128(quote + quoteToPool, quote + quoteToPool) },
            .denominator { Prod128(quote, base) }
        };
    }

    Ratio128 price_ratio_added_base(uint64_t baseToPool) const
    {
        return {
            .numerator { Prod128(base, quote) },
            .denominator { Prod128(base + baseToPool, base + baseToPool) }
        };
    }

    // relation of pool price (affected by adding given quoteToPool) to given price
    [[nodiscard]] std::strong_ordering rel_quote_price(uint64_t quoteToPool,
        Price p) const
    {
        return compare_fraction(price_ratio_added_quote(quoteToPool), p);
    }

    // relation of pool price (affected by pushing given baseToPool) to given price
    [[nodiscard]] std::strong_ordering rel_base_price(uint64_t baseToPool,
        Price p) const
    {
        return compare_fraction(price_ratio_added_base(baseToPool), p);
    }

    [[nodiscard]] bool modified_pool_price_exceeds(const Delta_uint64& toPool, Price p) const
    {
        if (toPool.isQuote)
            return rel_quote_price(toPool.amount, p) == std::strong_ordering::greater;
        else
            return rel_base_price(toPool.amount, p) != std::strong_ordering::less;
    }
};

inline BaseQuote_uint64 Delta_uint64::base_quote() const
{
    if (isQuote)
        return { 0, amount };
    return { amount, 0 };
}

struct FillResult_uint64 {
    std::optional<Delta_uint64> toPool;
    BaseQuote_uint64 filled;
};

struct MatchResult_uint64 : public FillResult_uint64 {
    size_t baseBound;
    size_t quoteBound;
};

namespace fair_batch_matching{
std::optional<Delta_uint64> balance_pool_interaction(const PoolLiquidity_uint64);
}

class FilledAndPool {
public:
    FilledAndPool(uint64_t basePool, uint64_t baseIn, uint64_t quotePool,
        uint64_t quoteIn)
        : in { baseIn, quoteIn }
        , pool { basePool, quotePool }
    {
    }
    std::optional<Delta_uint64> balance_pool_interaction() const;

    BaseQuote_uint64 in;
    PoolLiquidity_uint64 pool;

    MatchResult_uint64 bisect_dynamic_price(size_t baseBound, size_t quoteBound) const
    {
        return { { balance_pool_interaction(), in }, baseBound, quoteBound };
    }
};

class Matcher : public FilledAndPool {
public:
    Matcher(uint64_t totalBasePush, uint64_t totalQuotePush, Pool_uint64& p)
        : FilledAndPool(p.base_total(), 0, p.quote_total(), 0)
        , toPool0 { false, totalBasePush } // TODO: initialize with pool
        , toPool1 { true, totalQuotePush }
    {
    }

    // bool result determines whether for the next bisection step we must (true) or must not (false)
    // - increase matched quote amount or
    // - decrease matched base amount or
    // - decrease price argument
    bool bisection_step(Price p)
    {
        Delta_uint64 toPool { in.excess(p) };
        if (!pool.modified_pool_price_exceeds(toPool, p)) {
            toPool0 = toPool;
            return true;
        } else {
            toPool1 = toPool;
            return false;
        }
    };

    FillResult_uint64 bisect_fixed_price(const bool isQuote,
        const uint64_t fill0,
        const uint64_t fill1, Price p)
    {
        auto v0 { fill0 };
        auto v1 { fill1 };
        auto& v { isQuote ? in.quote : in.base };
        while (v1 + 1 != v0 && v0 + 1 != v1) {
            v = (v0 + v1) / 2;

            if (bisection_step(p))
                v0 = v;
            else
                v1 = v;
        }

        v = (toPool0.isQuote ? v0 : v1);
        auto toPool { [&]() -> std::optional<Delta_uint64> {
            auto& ref { toPool0.isQuote ? toPool0 : toPool1 };
            if (ref.amount == 0)
                return {};
            return ref;
        }() };

        return { .toPool { toPool }, .filled { in } };
    };
    Delta_uint64 toPool0;
    Delta_uint64 toPool1;
};
}
