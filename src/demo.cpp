#include "defi/defi.hpp"
#include "funds.hpp"
#include "nlohmann/json.hpp"
#include <emscripten.h>
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
  json buys(json::array());

  auto fquote{match_res.filled().quote};
  for (size_t i = 0; i < bso.quote_desc_buy().size(); ++i) {
    auto elem{bso.quote_desc_buy()[i]};
    Funds filled{std::min(elem.amount(), fquote)};
    fquote -= filled;
    buys.push_back({{"amount", elem.amount().to_string()},
                    {"filled", filled.to_string()},
                    {"limit", elem.limit().to_double()}});
  }
  json sells(json::array());
  auto J{bso.base_asc_sell().size()};

  auto fbase{match_res.filled().base};
  for (size_t j = 0; j < J; ++j) {
    auto order{bso.base_asc_sell()[j]};
    Funds filled{std::min(fbase, order.amount())};
    fbase -= filled;
    sells.push_back({{"amount", order.amount().to_string()},
                     {"filled", filled.to_string()},
                     {"limit", order.limit().to_double()}});
  }
  std::reverse(sells.begin(), sells.end());

  auto json_price{[](const defi::BaseQuote &bq) -> json {
    if (auto o{bq.price_double()})
      return json(*o);
    return json(nullptr);
  }};

  const auto &toPool{match_res.to_pool()};
  auto poolBaseQuote{[&]() -> defi::BaseQuote {
    if (toPool) {
      if (toPool->is_quote())
        return {pTmp.buy(toPool->amount(), 0), toPool->amount()};
      else {
        return {
            toPool->amount(),
            pTmp.sell(toPool->amount(), 0),
        };
      }
    }
    return {};
  }()};

  auto matched{match_res.filled()};
  if (match_res.to_pool())
    matched.subtract_throw(*match_res.to_pool());
  auto toPoolJson = [&]() -> json {
    if (toPool) {
      return {{"isQuote", toPool->is_quote()},
              {"base", poolBaseQuote.base.to_string()},
              {"quote", poolBaseQuote.quote.to_string()},
              {"price", json_price(poolBaseQuote)}};
    };
    return nullptr;
  };
  defi::BaseQuote filledBuyer{matched};
  defi::BaseQuote filledSeller{matched};
  if (toPool) {
    if (toPool->is_quote()) {
      filledBuyer.add_throw(poolBaseQuote);
    } else {
      filledSeller.add_throw(poolBaseQuote);
    }
  }

  return json{{"parseErrors", errors},
              {"match",
               {{"buys", buys},
                {"sells", sells},
                {"poolBefore", pool_json(p)},
                {"toPool", toPoolJson()},
                {"filled",
                 {{"outBaseSeller", filledSeller.base.to_string()},
                  {"inQuoteSeller", filledSeller.quote.to_string()},
                  {"priceSeller", json_price(filledSeller)},
                  {"indexBoundSeller", match_res.base_bound()},
                  {"outQuoteBuyer", filledBuyer.quote.to_string()},
                  {"inBaseBuyer", filledBuyer.base.to_string()},
                  {"priceBuyer", json_price(filledBuyer)},
                  {"indexBoundBuyer", match_res.quote_bound()}}},
                {"matched",
                 {{"base", matched.base.to_string()},
                  {"quote", matched.quote.to_string()},
                  {"price", matched.quote.is_zero() ? json(nullptr)
                                                    : json_price(matched)}}},
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

json delete_order(json j) {
  try {
    bool base = j["base"].get<bool>();
    auto i = j["index"].get<size_t>();
    bso.delete_index(base, i);
  } catch (...) {
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

EMSCRIPTEN_KEEPALIVE
const char *deleteOrder(const char* json) { return wrap_fun(delete_order, json); }
}
