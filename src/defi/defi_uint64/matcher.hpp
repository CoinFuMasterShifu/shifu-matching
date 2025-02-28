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

    Ratio128 add_quote_price_ratio(uint64_t quoteToPool) const
    {
        return {
            .numerator { Prod128(quote + quoteToPool, quote + quoteToPool) },
            .denominator { Prod128(quote, base) }
        };
    }

    Ratio128 add_base_price_ratio(uint64_t baseToPool) const
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
        return compare_fraction(add_quote_price_ratio(quoteToPool), p);
    }

    // relation of pool price (affected by pushing given baseToPool) to given price
    [[nodiscard]] std::strong_ordering rel_base_price(uint64_t baseToPool,
        Price p) const
    {
        return compare_fraction(add_base_price_ratio(baseToPool), p);
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

class Evaluator {
    static std::pair<std::strong_ordering, Prod192> take_smaller(Prod192& a,
        Prod192& b)
    {
        auto rel { a <=> b };
        if (rel == std::strong_ordering::less)
            return { rel, a };
        return { rel, b };
    }

public:
    Evaluator(uint64_t basePool, uint64_t baseIn, uint64_t quotePool,
        uint64_t quoteIn)
        : in { baseIn, quoteIn }
        , pool { basePool, quotePool }
    {
    }

    BaseQuote_uint64 in;
    PoolLiquidity_uint64 pool;
    struct ret_t {
        std::strong_ordering rel;
        using Pool128Ratio = Ratio128;
        struct Fill64Ratio {
            uint64_t a, b;
        };
        bool exceeded() const { return std::holds_alternative<Ratio128>(v); }
        auto& get_unexceeded_ratio() const { return std::get<Fill64Ratio>(v); };
        auto& get_exceeded_ratio() const { return std::get<Ratio128>(v); };
        std::variant<Ratio128, Fill64Ratio> v;
    };

    [[nodiscard]] ret_t rel_base_asc(uint64_t baseDelta) const
    {
        // pool price after we swap `baseDelta` from base to quote (sell) at the pool
        auto poolRatio { pool.add_base_price_ratio(baseDelta) };

        // if we subtract `baseDelta` from `in.base`,
        // new in price is in_numerator/in_denominator
        auto in_numerator { in.quote };
        auto in_denominator { in.base - baseDelta };

        // to compare the prices we compare these products
        auto pool_price_score { poolRatio.numerator * in_denominator };
        auto in_price_score { poolRatio.denominator * in_numerator };

        // as we push more base from in to pool (i.e. sell), the pool price will at some point
        // be smaller than the in price, to make the relation ascending (less -> equal -> greater)
        // in the argument `baseDelta` we compare in_price_score <=> pool_price_score.
        auto rel { in_price_score <=> pool_price_score };

        // the second return argument shall balance the two converged options of different relation,
        // we will pick the one that maximizes the min price
        // -> save the min price in second argument.
        // We swap denominator and numerator to avoid case distinction for comparison
        // ("maximize min price" for rel_base_asc, "minimize max price" for rel_quote_asc)
        // on callser side
        if (rel == std::strong_ordering::greater) // in price greater
            return { rel, ret_t::Pool128Ratio { poolRatio.denominator, poolRatio.numerator } };
        else
            return { rel, ret_t::Fill64Ratio { in_denominator, in_numerator } };
    }
    [[nodiscard]] ret_t rel_quote_asc(uint64_t quoteToPool) const
    {
        // pool price after we swap `quoteToPool` from quote to base (buy) at the pool
        auto poolRatio { pool.add_quote_price_ratio(quoteToPool) };


        // if we subtract `quoteToPool` from `in.quote`,
        // new in price is in_numerator/in_denominator
        auto in_numerator { in.quote - quoteToPool };
        auto in_denominator { in.base };

        // to compare the prices we compare these products
        auto pool_price_score { poolRatio.numerator * in_denominator };
        auto in_price_score { poolRatio.denominator * in_numerator };

        // as we push more quote from in to pool (i.e. buy), the pool price will at some point
        // be greater than the in price, to make the relation ascending (less -> equal -> greater)
        // in the argument `quoteToPool` we compare pool_price_score <=> in_price_score.
        auto rel { pool_price_score <=> in_price_score };

        // the second return argument shall balance the two converged options of different relation,
        // we will pick the one that minimizes the max price -> save the max price in second argument
        if (rel == std::strong_ordering::greater)
            return { rel, ret_t::Pool128Ratio { poolRatio } };
        else
            return { rel, ret_t::Fill64Ratio { in_numerator, in_denominator } };
    }

    MatchResult_uint64 bisect_dynamic_price(size_t baseBound, size_t quoteBound) const
    {
        auto bisect = [](Evaluator::ret_t::Fill64Ratio ratio0, uint64_t v1,
                          auto asc_fun) {
            using ret_t = Evaluator::ret_t;
            if (v1 == 0)
                return v1;
            ret_t r { asc_fun(v1) };
            assert(r.rel != std::strong_ordering::less);
            if (r.rel == std::strong_ordering::equal)
                return v1;
            auto ratio1 { r.get_exceeded_ratio() };
            uint64_t v0 { 0 };
            while (v0 + 1 < v1) {
                uint64_t v { (v1 + v0) / 2 };
                ret_t ret = asc_fun(v);
                if (ret.exceeded()) {
                    v1 = v;
                    ratio1 = ret.get_exceeded_ratio();
                } else {
                    v0 = v;
                    ratio0 = ret.get_unexceeded_ratio();
                }
            }
            if (ratio1.denominator * ratio0.a < ratio1.numerator * ratio0.b)
                return v0;
            return v1;
        };
        auto baseRet { rel_base_asc(0) };
        auto quoteRet { rel_quote_asc(0) };
        auto make_toPool = [&](bool isQuote,
                               uint64_t toPool) -> std::optional<Delta_uint64> {
            if (toPool == 0)
                return {};
            return Delta_uint64 { isQuote, toPool };
        };
        if (baseRet.rel == std::strong_ordering::greater) {
            // need to push quote to pool
            assert(quoteRet.rel == std::strong_ordering::less);
            uint64_t toPoolAmount {
                bisect(quoteRet.get_unexceeded_ratio(), in.quote,
                    [&](uint64_t toPool) { return rel_quote_asc(toPool); })
            };
            auto toPool { make_toPool(true, toPoolAmount) };
            return { { toPool, in }, baseBound, quoteBound };
        } else {
            assert(quoteRet.rel != std::strong_ordering::less);
            // need to push base to pool
            auto toPoolAmount {
                bisect(baseRet.get_unexceeded_ratio(), in.base,
                    [&](uint64_t toPool) { return rel_base_asc(toPool); })
            };
            auto toPool { make_toPool(false, toPoolAmount) };
            return { { toPool, in }, baseBound, quoteBound };
        }
    }
};

class Matcher : public Evaluator {
public:
    Matcher(uint64_t totalBasePush, uint64_t totalQuotePush, Pool_uint64& p)
        : Evaluator(p.base_total(), 0, p.quote_total(), 0)
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
