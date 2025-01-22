#include "defi/defi.hpp"
#include <iomanip>
#include <iostream>

using namespace std;
using namespace defi;

namespace test {
void print_from_fraction() {
  cout << (PriceRelative::from_fraction(2, 3) < *Price::from_double(0.6))
       << endl;
  cout << (PriceRelative::from_fraction(2, 3) < *Price::from_double(0.7))
       << endl;

  cout << (PriceRelative::from_fraction(3, 2) < *Price::from_double(1.4))
       << endl;
  cout << (PriceRelative::from_fraction(3, 2) < *Price::from_double(1.5))
       << endl;
  cout << (PriceRelative::from_fraction(3, 2) < *Price::from_double(1.6))
       << endl;
}
void multiply_floor() {
  auto p{Price::from_double(0.0991).value()};
  cout << std::setprecision(15) << p.to_double() << endl;
  auto pr = [](uint64_t a, Price p) {
    cout << a << "*" << p.to_double() << " = " << ::multiply_floor(a, p).value()
         << endl;
  };
  pr(100ull, p);
}
void from_fraction() {
  auto p{Price::from_double(0.0991).value()};
  cout << std::setprecision(15) << p.to_double() << endl;
  auto print = [](uint64_t a, uint64_t b) {
    auto pr{PriceRelative::from_fraction(a, b)};
    cout << a << "/" << b << " in [" << pr.price.to_double() << ", "
         << pr.ceil()->to_double() << "]";
  };
  auto a{12311231212313122ull};
  print(a, a + 1);
}
} // namespace test

std::ostream &operator<<(std::ostream &os, const Funds &f) {
  return os << f.to_string();
}
void print_match(BuySellOrders &bso, Pool &p) {
  auto res{bso.match(p)};
  auto tp{res.to_pool()};
  if (tp) {
    cout << "to pool: " << tp->amount() << " ("
         << (tp->is_quote() ? "quote" : "base") << ")\n";
  }
  cout << "Price: (Pool before): " << p.price().price.to_double() << endl;
  if (tp) {
    if (tp->is_quote()) {
      p.buy(tp->amount());
    } else {
      p.sell(tp->amount());
    }
  }
  cout << "Price (Pool after):  " << p.price().price.to_double() << endl;
  auto nf{res.not_filled()};
  if (nf) {
    cout << "Not filled: " << nf->amount() << " ("
         << (nf->is_quote() ? "quote" : "base") << ")" << endl;
  }
  auto matched{tp ? res.filled() - tp->base_quote() : res.filled()};
  if (!matched.base.is_zero()) {
    cout << "Price (matched):     " << matched.price().price.to_double()
         << endl;
  }
  cout << "matched (base/quote): (" << matched.base << "/" << matched.quote
       << ")" << endl;
  cout << "filled (base/quote):  (" << res.filled().base << "/"
       << res.filled().quote << ")" << endl;
}
int main() {
  using namespace defi;
  auto coins = [](uint32_t amount) {
    return Funds::from_value_throw(uint64_t(amount) * 100000000);
  };
  Pool p(coins(100), coins(200));
  cout << "Pool " << p.base_total() << " " << p.quote_total() << endl;
  BuySellOrders bso;
  bso.insert_base(
      Order(coins(100), Price::from_double(1).value()));
  bso.insert_base(
      Order(coins(100), Price::from_double(0.5).value()));
  bso.insert_quote(
      Order(coins(100), Price::from_double(4).value()));
  bso.insert_quote(
      Order(coins(100), Price::from_double(3).value()));
  print_match(bso, p);
}
