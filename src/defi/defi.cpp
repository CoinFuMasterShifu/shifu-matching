#include "defi.hpp"
#include "uint64/lazy_matching.hpp"
#include <iostream>

using namespace std;
void print_res(defi::MatchResult_uint64& r)
{
    cout << "filled.base: " << r.filled.base.value() << endl;
    cout << "filled.quote: " << r.filled.quote.value() << endl;
    if (r.toPool) {
        cout << "r.toPool (" << (r.toPool->isQuote ? "quote" : "base") << "): " << r.toPool->amount.value() << endl;
    }
}

namespace defi {
MatchResult BuySellOrders::match_assert_lazy(Pool& p)
{
    auto res { buySellOrders_uint64.match(p.pool_uint64) };

    struct Loader {
        size_t i { 0 };
        const SortedOrderVector_uint64& v;
        Loader(const SortedOrderVector_uint64& v)
            : v(v)
        {
        }
        std::optional<Price_uint64> next_price()
        {
            if (i == v.size())
                return {};
            return v[i].limit;
        }
        Order_uint64 load_next_order()
        {
            assert(i != v.size());
            return v[i++];
        }
    };
    Loader loaderBaseAsc{buySellOrders_uint64.base_asc_sell()};
    Loader loaderQuoteDesc{buySellOrders_uint64.quote_desc_buy()};

    auto res2 { match_lazy(loaderBaseAsc, loaderQuoteDesc, p.pool_uint64) };
    bool equal{res == res2};
    if (!equal) {
        cout << "res: " << endl;
        print_res(res);
        cout << "res2: " << endl;
        print_res(res2);
        assert(res == res2); // verify that indeed lazy matching is correct
    }
    return res;
}
}
