#pragma once
#include <boost/asio.hpp>

namespace nodecxx {
namespace core {
extern boost::asio::io_service service;
} // namespace core

void run(unsigned numThreads = 1);

} // namespace nodecxx

