#include <core.hpp>
#include "http.hpp"
#include "http_parser.h"

using namespace boost::asio;

namespace nodecxx {

HttpServer::HttpServer()
{} 

void HttpServer::listen(const std::string& port, const std::string& host)
{
    server.on(connection, [this](auto& socket) { openedConnection(socket); });
}

template<class Protocol>
void HttpServer::openedConnection(Socket<Protocol>& socket) {
    auto parser = new http_parser();
    auto settings = new http_parser_settings();
    ::http_parser_init(parser, ::HTTP_REQUEST);
    socket.on(data, [parser, settings](const std::vector<char>& data) {
        ::http_parser_execute(parser, settings, data.data(), data.size());
    });
}

} // namespace nodecxx

