#include "defi/defi.hpp"
#include <iomanip>
#include <iostream>

using namespace std;
using namespace defi;

namespace test {
void print_from_fraction() {
  cout << (PriceRelative_uint64::from_fraction(2, 3) < *Price_uint64::from_double(0.6))
       << endl;
  cout << (PriceRelative_uint64::from_fraction(2, 3) < *Price_uint64::from_double(0.7))
       << endl;

  cout << (PriceRelative_uint64::from_fraction(3, 2) < *Price_uint64::from_double(1.4))
       << endl;
  cout << (PriceRelative_uint64::from_fraction(3, 2) < *Price_uint64::from_double(1.5))
       << endl;
  cout << (PriceRelative_uint64::from_fraction(3, 2) < *Price_uint64::from_double(1.6))
       << endl;
}
void multiply_floor() {
  auto p{Price_uint64::from_double(0.0991).value()};
  cout << std::setprecision(15) << p.to_double() << endl;
  auto pr = [](uint64_t a, Price_uint64 p) {
    cout << a << "*" << p.to_double() << " = " << ::multiply_floor(a, p).value()
         << endl;
  };
  pr(100ull, p);
}
void from_fraction() {
  auto p{Price_uint64::from_double(0.0991).value()};
  cout << std::setprecision(15) << p.to_double() << endl;
  auto print = [](uint64_t a, uint64_t b) {
    auto pr{PriceRelative_uint64::from_fraction(a, b).value()};
    cout << a << "/" << b << " in [" << pr.floor().to_double() << ", "
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
  auto res{bso.match_assert_lazy(p)};
  auto tp{res.to_pool()};
  if (tp) {
    cout << "to pool: " << tp->amount() << " ("
         << (tp->is_quote() ? "quote" : "base") << ")\n";
  }
  cout << "Price_uint64: (Pool before): " << p.price().value().floor().to_double() << endl;
  if (tp) {
    if (tp->is_quote()) {
      p.buy(tp->amount());
    } else {
      p.sell(tp->amount());
    }
  }
  cout << "Price (Pool after):  " << p.price().value().floor().to_double() << endl;
  auto matched{tp ? res.filled() - tp->base_quote() : res.filled()};
  if (!matched.base.is_zero()) {
    cout << "Price (matched):     " << matched.price().value().floor().to_double()
         << endl;
  }
  cout << "matched (base/quote): (" << matched.base << "/" << matched.quote
       << ")" << endl;
  cout << "filled (base/quote):  (" << res.filled().base << "/"
       << res.filled().quote << ")" << endl;
}
int main() {
    auto base0{Funds::from_value_throw(  10000000000)};
    auto quote0{Funds::from_value_throw(100000000100)};
    auto baseSell{Funds::from_value_throw(4)};
    cout<<"Base0: "<<base0.to_string()<<endl;
    cout<<"quote0: "<<quote0.to_string()<<endl;
    cout<<"baseSell: "<<baseSell.to_string()<<endl;
    Pool p(base0, quote0);
    auto quoteReturn{p.sell(baseSell,1)};
    cout<<"quoteReturn: "<<quoteReturn.to_string()<<endl;

  //
  // using namespace defi;
  // auto coins = [](uint32_t amount) {
  //   return Funds::from_value_throw(uint64_t(amount) * 100000000);
  // };
  // Pool p(coins(100), coins(200));
  // cout << "Pool " << p.base_total() << " " << p.quote_total() << endl;
  // BuySellOrders bso;
  // bso.insert_base(
  //     Order(coins(100), Price_uint64::from_double(1).value()));
  // bso.insert_base(
  //     Order(coins(100), Price_uint64::from_double(0.5).value()));
  // bso.insert_quote(
  //     Order(coins(100), Price_uint64::from_double(4).value()));
  // bso.insert_quote(
  //     Order(coins(100), Price_uint64::from_double(3).value()));
  // print_match(bso, p);
}
