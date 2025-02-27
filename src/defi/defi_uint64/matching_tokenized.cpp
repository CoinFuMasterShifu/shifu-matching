#include "matching_tokenized.hpp"
#include "src/defi/defi_uint64/pool.hpp"

namespace defi {
namespace matching_tokenized {
    struct OrderExtra {
        Order_uint64 order;
        uint64_t cumsum { 0 };
    };
    struct OrderCache {
    private:
        bool finished { false };
        order_generator _generate;
        uint64_t cumsum { 0 };

        std::vector<OrderExtra> v;

    public:
        auto& orders() const { return v; }
        OrderExtra* load_next()
        {
            if (finished)
                return nullptr;
            auto order { _generate() };
            if (!order) {
                finished = true;
                return nullptr;
            }
            cumsum += order->amount;
            v.push_back({ .order { *order }, .cumsum = cumsum });
            return &v.back();
        };
    };
    struct Matcher {
        OrderCache buys;
        OrderCache sells;
        void match(Pool_uint64 pool)
        {
            const auto price { pool.price() };

            while (true) {
                auto o { buys.load_next() };
                if (!o || o->order.limit > price)
                    break;
            }

            // sells
            while (true) {
                auto o { sells.load_next() };
                if (!o || o->order.limit < price)
                    break;
            }
        }
    };
};
} // namespace matching_tokenized
} // namespace defi
