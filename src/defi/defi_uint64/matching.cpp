#include "matching.hpp"
#include "pool.hpp"
namespace defi {
auto BuySellOrders_uint64::match(Pool_uint64& p)
    -> MatchResult_uint64
{
    const size_t I { pushQuoteDesc.size() };
    const size_t J { pushBaseAsc.size() };
    size_t i0 { 0 };
    size_t i1 { I };

    Matcher m { pushBaseAsc.total_push(), pushQuoteDesc.total_push(), p };

    while (i0 != i1) {
        auto i { (i0 + i1) / 2 };
        auto& eq { extraQuote[i] };
        auto j { eq.upperBoundCounterpart };
        m.in.base = (j == J ? 0 : extraBase[j].cumsum);
        m.in.quote = eq.cumsum;
        if (m.bisection_step(pushQuoteDesc[i].limit))
            i0 = i + 1;
        else
            i1 = i;
    }
    auto bisect_j = [&](size_t j0, size_t j1) -> MatchResult_uint64 {
        while (j0 != j1) {
            auto j { (j0 + j1) / 2 };
            m.in.base = extraBase[j].cumsum;
            if (m.bisection_step(pushBaseAsc[J - 1 - j].limit))
                j0 = j + 1;
            else
                j1 = j;
        }
        if (j1 == 0) {
            return m.bisect_dynamic_price(J - j1, i1);
        } else {
            auto j { j1 - 1 };
            m.in.base = extraBase[j].cumsum - pushBaseAsc[J - 1 - j].amount;
            auto price { pushBaseAsc[J - 1 - j].limit };
            if (m.bisection_step(price)) {
                return m.bisect_dynamic_price(J - j1, i1);
            } else {
                return {
                    m.bisect_fixed_price(false, extraBase[j].cumsum, m.in.base, price),
                    J - j1, i1
                };
            }
        }
    };
    if (i1 == 0) {
        size_t j0 = 0;
        size_t j1 = (I == 0 ? J : extraQuote[0].upperBoundCounterpart);
        m.in.quote = 0;
        return bisect_j(j0, j1);
    } else {
        auto i { i1 - 1 };
        auto& eq { extraQuote[i] };
        auto price { pushQuoteDesc[i].limit };
        auto j { eq.upperBoundCounterpart };
        m.in.base = (j == J ? 0 : extraBase[j].cumsum);
        m.in.quote = eq.cumsum + pushQuoteDesc[i].amount;
        size_t j0 = extraQuote[i].upperBoundCounterpart;
        if (m.bisection_step(price)) {
            size_t j1 = (i1 < I ? extraQuote[i1].upperBoundCounterpart : J);
            return bisect_j(j0, j1);
        } else {
            return { m.bisect_fixed_price(true, eq.cumsum, m.in.quote, price), J - j0,
                i };
        }
    }
}
} // namespace defi
