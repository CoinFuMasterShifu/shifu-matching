#pragma once
#include "pool.hpp"
#include "types.hpp"

#include "src/defi/price.hpp"

namespace defi {

struct FillResult_uint64 {
    std::optional<Delta_uint64> toPool;
    BaseQuote_uint64 filled;
};

using MatchResult_uint64 = FillResult_uint64;

namespace fair_batch_matching {
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

    MatchResult_uint64 bisect_dynamic_price() const
    {
        return { balance_pool_interaction(), in };
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
