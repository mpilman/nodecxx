#include <boost/asio.hpp>
#include <thread>
#include <iostream>

#include "core.hpp"

namespace nodecxx {
namespace core {

boost::asio::io_service& service() {
    static boost::asio::io_service res;
    return res;
}

} // namespace core

void run(unsigned numThreads) {
    std::vector<std::thread> threads;
    threads.reserve(numThreads);
    for (unsigned i = 0; i < numThreads - 1; ++i) {
        threads.emplace_back([]() { core::service().run(); });
    }
    std::cout << "io_service run\n";
    core::service().run();
    for (auto& t : threads) t.join();
    std::cout << "done\n";
}

} // namespace nodecxx

