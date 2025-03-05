#pragma once
#include <compare>
#include <cstdint>

class Writer;
struct IsUint64 {
public:
    explicit IsUint64(int64_t w);
    explicit IsUint64(int w)
        : IsUint64((int64_t)(w)) {};
    explicit constexpr IsUint64(uint64_t val)
        : val(val) {};

    bool operator==(const IsUint64&) const = default;
    auto operator<=>(const IsUint64&) const = default;
    friend Writer& operator<<(Writer& w, const IsUint64& v);

    uint64_t value() const
    {
        return val;
    }

protected:
    uint64_t val;
};

