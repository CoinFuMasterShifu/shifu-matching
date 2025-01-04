#pragma once
#include "funds.hpp"
#include "price.hpp"
namespace defi{
    struct Order {
        uint64_t amount;
        Price limit;
        Order(Funds amount, Price limit) : amount(amount.E8()), limit(limit) {}
    };
    struct PullBaseOrder : public Order { // sells quote
        using Order::Order;
    };
    struct PullQuoteOrder : public Order { // sells base
        using Order::Order;
    };
    struct PushBaseOrder : public Order { // sells base
        using Order::Order;
    };
    struct PushQuoteOrder : public Order { // sells quote
        using Order::Order;
    };
}
