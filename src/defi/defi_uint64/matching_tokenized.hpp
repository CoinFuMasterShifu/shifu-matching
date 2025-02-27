#pragma once
#include "src/defi/defi_uint64/order.hpp"
#include <functional>

namespace defi {
namespace matching_tokenized {

using order_generator = std::function<std::optional<Order_uint64>()>;


}
} // namespace defi
