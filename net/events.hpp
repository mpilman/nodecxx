#pragma once
#include <functional>
#include <boost/system/error_code.hpp>

namespace nodecxx {

struct close_t {
    using function_type = std::function<void(bool)>;
    constexpr close_t() {}
};

constexpr close_t close;

struct data_t {
    using function_type = std::function<void(const char*, size_t)>;
    constexpr data_t() {}
};

constexpr data_t data;

struct error_t {
    using function_type = std::function<void(const boost::system::error_code&)>;
    constexpr error_t() {}
};

constexpr error_t error; 

struct drain_t {
    using function_type = std::function<void()>;
    constexpr drain_t() {}
};

constexpr drain_t drain;

} // namespace nodecxx

