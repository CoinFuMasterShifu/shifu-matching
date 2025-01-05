#pragma once
#include "../price.hpp"
namespace defi{
    struct Order_uint64 {
        uint64_t amount;
        Price limit;
    };
    struct PullBaseOrder : public Order_uint64 { // sells quote
        using Order_uint64::Order_uint64;
    };
    struct PullQuoteOrder : public Order_uint64 { // sells base
        using Order_uint64::Order_uint64;
    };
    struct PushBaseOrder : public Order_uint64 { // sells base
        using Order_uint64::Order_uint64;
    };
    struct PushQuoteOrder : public Order_uint64 { // sells quote
        using Order_uint64::Order_uint64;
    };
}
