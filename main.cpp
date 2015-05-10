#include <iostream>

#include "net/net.hpp"
#include "core.hpp"

using namespace nodecxx;

int main()
{
    auto server = server::createServer([](auto& socket){
        std::cout << "Opened a connection" << std::endl;
        socket.on(error, [&socket](const boost::system::error_code& ec) {
            std::cerr << "Got an error:" << ec.message() << std::endl;
            socket.close();
        });
        socket.on(data, [&socket](const std::vector<char>& data) {
            auto str = std::string(data.begin(), data.end());
            std::cout << str;
            if (str == "close\r\n")
                socket.end(std::vector<char>(data.begin(), data.end()));
            else
                socket.write(std::vector<char>(data.begin(), data.end()));
        });
    });
    server.listen("8713", "localhost");
    run();
}
