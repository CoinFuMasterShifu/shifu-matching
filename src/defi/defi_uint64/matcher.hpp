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
    auto price() const { return PriceRelative::from_fraction(quote, base); }
};
inline BaseQuote_uint64 Delta_uint64::base_quote() const
{
    if (isQuote)
        return { 0, amount };
    return { amount, 0 };
}

struct FillResult_uint64 {
    std::optional<Delta_uint64> toPool;
    std::optional<Delta_uint64> notFilled;
    BaseQuote_uint64 filled;
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
        auto& get_unexceeded_ratio() const { return std::get<Fill64Ratio>(v); };
        auto& get_exceeded_ratio() const { return std::get<Pool128Ratio>(v); };
        std::variant<Pool128Ratio, Fill64Ratio> v;
    };

    [[nodiscard]] ret_t rel_base_asc(uint64_t baseDelta) const
    {
        auto pp1 { Prod128(pool.base, pool.quote) };
        auto pp2 { in.base - baseDelta };
        auto pp { pp1 * pp2 };
        auto op1 { Prod128(pool.base + baseDelta, pool.base + baseDelta) };
        auto op2 { in.quote };
        auto op { op1 * op2 };

        auto rel { op <=> pp };
        if (rel == std::strong_ordering::greater) {
            return { rel, ret_t::Pool128Ratio { pp1, op1 } };
        }
        return { rel, ret_t::Fill64Ratio { pp2, op2 } };
    }
    [[nodiscard]] ret_t rel_quote_asc(uint64_t quoteToPool) const
    {
        auto pp1 { Prod128(pool.quote, pool.base) };
        auto pp2 { in.quote - quoteToPool };
        auto pp { pp1 * pp2 };
        auto op1 { Prod128(pool.quote + quoteToPool, pool.quote + quoteToPool) };
        auto op2 { in.base };
        auto op { op1 * op2 };
        auto rel { op <=> pp };
        if (rel == std::strong_ordering::greater) {
            return { rel, ret_t::Pool128Ratio { pp1, op1 } };
        }
        return { rel, ret_t::Fill64Ratio { pp2, op2 } };
    }

    [[nodiscard]] std::strong_ordering rel_quote_price(uint64_t quoteToPool,
        Price p) const
    {
        return compare_fraction(
            Prod128(pool.quote + quoteToPool, pool.quote + quoteToPool),
            Prod128(pool.base, pool.quote), p);
    }
    [[nodiscard]] std::strong_ordering rel_base_price(uint64_t baseToPool,
        Price p) const
    {
        return compare_fraction(
            Prod128(pool.base, pool.quote),
            Prod128(pool.base + baseToPool, pool.base + baseToPool), p);
    }
    [[nodiscard]] Prod192 quotedelta_quadratic_asc(uint64_t quoteDelta) const
    {
        return Prod128(pool.quote + quoteDelta, pool.quote + quoteDelta) * in.base;
    }
};

struct MatchResult_uint64 : public FillResult_uint64 {
    size_t baseBound;
    size_t quoteBound;
};

class Matcher : public Evaluator {
public:
    Matcher(uint64_t totalBasePush, uint64_t totalQuotePush, Pool_uint64& p)
        : Evaluator(p.base_total(), 0, p.quote_total(), 0)
        , toPool0 { false, totalBasePush } // TODO: initialize with pool
        , toPool1 { true, totalQuotePush }
    {
    }
    Delta_uint64 order_excess(Price p) const
    {
        if (quoteIntoPool1 == true) {
            auto q { multiply_floor(in.base, p) };
            if (q.has_value() && *q <= in.quote) // too much base
                return { true, in.quote - *q };
        }
        auto b { divide_floor(in.quote, p) };
        assert(b.has_value()); // TODO: verify assert by checking precision of
                               // divide_floor
        assert(*b <= in.base);
        return { false, in.base - *b };
    }
    bool needs_increase(Price p)
    {
        Delta_uint64 toPool { order_excess(p) };
        bool needsIncrease {
            toPool.isQuote ? rel_quote_price(toPool.amount, p) != std::strong_ordering::greater
                           : rel_base_price(toPool.amount, p) == std::strong_ordering::less
        };
        if (needsIncrease)
            toPool0 = toPool;
        else
            toPool1 = toPool;
        return needsIncrease;
    };

    MatchResult_uint64 bisect_dynamic_price(size_t baseBound, size_t quoteBound)
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
            if (ratio1.b * ratio0.a < ratio1.a * ratio0.b)
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
            assert(quoteRet.rel != std::strong_ordering::greater); // TODO: verify
            uint64_t toPoolAmount {
                bisect(quoteRet.get_unexceeded_ratio(), in.quote,
                    [&](uint64_t toPool) { return rel_quote_asc(toPool); })
            };
            auto toPool { make_toPool(true, toPoolAmount) };
            return { { toPool, {}, in }, baseBound, quoteBound };
        } else {
            assert(baseRet.rel != std::strong_ordering::greater); // TODO: verify
            // need to push base to pool
            auto toPoolAmount {
                bisect(baseRet.get_unexceeded_ratio(), in.base,
                    [&](uint64_t toPool) { return rel_base_asc(toPool); })
            };
            auto toPool { make_toPool(false, toPoolAmount) };
            return { { toPool, {}, in }, baseBound, quoteBound };
        }
    }
    FillResult_uint64 bisect_fixed_price(const bool isQuoteOrder,
        const uint64_t order0,
        const uint64_t order1, Price p)
    {
        auto v0 { order0 };
        auto v1 { order1 };
        auto& v { isQuoteOrder ? in.quote : in.base };
        while (v1 + 1 != v0 && v0 + 1 != v1) {
            v = (v0 + v1) / 2;
            if (needs_increase(p))
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

        auto nf { std::max(order0, order1) - v };
        std::optional<Delta_uint64> notFilled;
        if (nf > 0) {
            notFilled = Delta_uint64 { isQuoteOrder, nf };
        }
        auto filled { isQuoteOrder ? BaseQuote_uint64 { in.base, v }
                                   : BaseQuote_uint64 { v, in.quote } };

        return { .toPool { toPool }, .notFilled { notFilled }, .filled { filled } };
    };
    bool quoteIntoPool1 { true };
    Delta_uint64 toPool0;
    Delta_uint64 toPool1;
};
}
