#pragma once
#include <net/net.hpp>
#include <functional>
#include "http_parser.h"

namespace nodecxx {

createEvent(request);

class HttpServer {
    TcpServer server;
public:
    HttpServer();
    void listen(const std::string& port, const std::string& host);
private:
    template<class Protocol>
    void openedConnection(Socket<Protocol>& socket);
};

class IncomingMessage {
    ::http_parser parser;
    ::http_parser_settings paserSettings;
};

} // namespace nodecxx
