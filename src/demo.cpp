#include "defi/defi.hpp"
#include "funds.hpp"
#include "nlohmann/json.hpp"
#include <emscripten.h>
#include <iostream>
#include <stdio.h>
using namespace std;

// global variables
std::string returnString;
std::optional<Funds> poolToken;
std::optional<Funds> poolWart;
defi::BuySellOrders bso;

using json = nlohmann::json;
json pool_json(const defi::Pool &pool) {
  return {{"base", pool.base_total().to_string()},
          {"quote", pool.quote_total().to_string()},
          {"price",
           double(pool.quote_total().E8()) / double(pool.base_total().E8())}};
}

json match_result() {
  json errors{
      {"poolToken", !poolToken.has_value()},
      {"poolWart", !poolWart.has_value()},
  };
  if (!poolToken || !poolWart)
    return {{"parseErrors", errors}};

  const defi::Pool p{*poolToken, *poolWart};
  auto pTmp{p};
  auto match_res{bso.match(pTmp)};
  const auto nf{match_res.not_filled()};
  json buys(json::array());
  for (size_t i = 0; i < bso.quote_desc_buy().size(); ++i) {
    bool matched{i < match_res.quote_bound()};
    auto elem{bso.quote_desc_buy()[i]};
    Funds filled{Funds::zero()};
    if (matched) {
      filled = elem.amount();
    }
    if (i == match_res.quote_bound() && nf && nf->is_quote()) {
      filled = Funds::diff_throw(elem.amount(), nf->amount());
    }
    buys.push_back({{"amount", elem.amount().to_string()},
                    {"filled", filled.to_string()},
                    {"limit", elem.limit().to_double()}});
  }
  json sells(json::array());
  auto J{bso.base_asc_sell().size()};
  cout << "match_res.baseBound" << match_res.base_bound() << endl;
  if (nf) {
    cout << "match_res.notFilled->amount" << nf->amount().to_string() << endl;
    ;
  }
  for (size_t j = 0; j < J; ++j) {
    bool matched{j < match_res.base_bound()};
    auto order{bso.base_asc_sell()[j]};
    Funds filled{Funds::zero()};
    if (matched) {
      filled = order.amount();
    }
    if (j == match_res.base_bound() && nf && !nf->is_base()) {
      filled = Funds::diff_throw(order.amount(), nf->amount());
    }
    sells.push_back({{"amount", order.amount().to_string()},
                     {"filled", filled.to_string()},
                     {"limit", order.limit().to_double()}});
  }
  std::reverse(sells.begin(), sells.end());

  const auto &toPool{match_res.to_pool()};
  auto poolSwapped{[&]() {
    if (toPool.is_quote())
      return pTmp.buy(toPool.amount(), 0);
    else
      return pTmp.sell(toPool.amount(), 0);
  }()};
  auto matched{match_res.filled().diff_throw(match_res.to_pool())};

  return {{"parseErrors", errors},
          {"match",
           {{"buys", buys},
            {"sells", sells},
            {"poolBefore", pool_json(p)},
            {"toPool",
             {{"isQuote", toPool.is_quote()},
              {"in", toPool.amount().to_string()},
              {"out", poolSwapped.to_string()}}},
            {"filled",
             {{"base", match_res.filled().base.to_string()},
              {"quote", match_res.filled().quote.to_string()}}},
            {"matched",
             {{"base", matched.base.to_string()},
              {"quote", matched.quote.to_string()}}},
            {"poolAfter", pool_json(pTmp)}}}};
}

template <typename callable>
  requires std::is_invocable_r_v<json, callable, json>
const char *wrap_fun(const callable &fun, const char *c) {
  returnString = [&]() {
    try {
      return fun(json::parse(std::string_view(c))).dump();
    } catch (std::runtime_error &e) {
      return json{{"error", e.what()}}.dump();
    }
  }();
  return returnString.c_str();
}

defi::Order parse_order(json j) {
  auto price{[&]() {
    try {
      return Price::from_string(j["price"].get<std::string>()).value();
    } catch (...) {
      throw std::runtime_error("Cannot get price");
    }
  }()};
  auto amount{[&]() {
    try {
      std::string s{j["amount"].get<std::string>()};
      if (auto o{Funds::parse(s)})
        return *o;

    } catch (...) {
    }
    throw std::runtime_error("Cannot get amount (must be a 64 bit integer)");
  }()};
  return {amount, price};
}

json edit_pool(json j) {
  try {
    poolToken = Funds::parse(j["token"].get<std::string>());
  } catch (...) {
    poolToken.reset();
  }
  try {
    poolWart = Funds::parse(j["wart"].get<std::string>());
  } catch (...) {
    poolWart.reset();
  }
  return match_result();
}

json add_buy(json j) {
  auto order{parse_order(j)};
  bso.insert_quote(order);
  return match_result();
}

json add_sell(json j) {
  auto order{parse_order(j)};
  bso.insert_base(order);
  return match_result();
}

extern "C" {
EMSCRIPTEN_KEEPALIVE
const char *addBuy(const char *json) { return wrap_fun(add_buy, json); }

EMSCRIPTEN_KEEPALIVE
const char *addSell(const char *json) { return wrap_fun(add_sell, json); }

EMSCRIPTEN_KEEPALIVE
const char *editPool(const char *json) { return wrap_fun(edit_pool, json); }
}
