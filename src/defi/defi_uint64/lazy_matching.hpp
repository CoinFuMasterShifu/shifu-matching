#pragma once

#include "src/defi/defi_uint64/order.hpp"
#include <functional>

namespace defi {

using OrderLoader = std::function<std::optional<Order_uint64>()>;
struct LazyLoadedOrders {
    struct Entry {
        Order_uint64 order;
        uint64_t cumsum;
    };
    std::optional<Price> load_next();
    std::vector<Entry> loaded;
    std::optional<Price> prevPrice;
    OrderLoader loader;
private:
    uint64_t cumsum{0};
};
struct LazyLoader {
    LazyLoadedOrders buyDesc;
    LazyLoadedOrders sellAsc;
};

struct LazyMatcher {
    OrderLoader loadBaseIncreasing;
    OrderLoader loadQuoteDecreasing;
};
}
