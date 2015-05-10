#pragma once

#define createEvent(name) struct name##_t { constexpr name##_t() {} }; constexpr name##_t name;

namespace nodecxx {

createEvent(close);
createEvent(data);
createEvent(error);
createEvent(drain);

} // namespace nodecxx

