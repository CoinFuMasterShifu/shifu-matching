#include "lazy_matching.hpp"
#include "src/defi/defi_uint64/pool.hpp"
#include "matcher.hpp"

namespace defi {

std::optional<Price> LazyLoadedOrders::load_next()
{
    auto o { loader() };
    if (!o) {
        prevPrice = {};
        return prevPrice;
    }
    loaded.push_back({ .order { *o }, .cumsum = cumsum });
    cumsum += o->amount;
    assert(prevPrice != o->limit); // we don't allow multiple orders at same price level
    prevPrice = o->limit;
    return prevPrice;
}

bool match_lazy(LazyLoader loader, Pool_uint64 pool)
{
    auto p { pool.price() };
    if (!p)
        return false;
    const auto price { *p };

    // load buy orders below pool price
    while (true) {
        auto p { loader.buyDesc.load_next() };
        if (!p || *p < price)
            break;
    }

    // load sell orders above pool price
    while (true) {
        auto p { loader.sellAsc.load_next() };
        if (!p || *p > price)
            break;
    }
    FilledAndPool fp;

}
}
