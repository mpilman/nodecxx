#include <core.hpp>
#include "http.hpp"

#include <chrono>
#include <iomanip>

using namespace boost::asio;
using namespace boost::system;

namespace nodecxx {

namespace {
// We need an implementation of IncomingMessage that can handle callbacks
// and is hidden from the user.

void getHttpDate(std::ostream& os) {
    os.imbue(std::locale("en_US.UTF-8"));
    std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    ::tm gtime;
    ::gmtime_r(&t, &gtime);
    os << std::put_time(&gtime, "%a, %d %b %Y %T GMT");
}

const char* defaultStatusMessage(int statusCode) {
    switch(statusCode) {
    case 100:
        return "Continue";
    case 101:
        return "Switching Protocols";
    case 200:
        return "OK";
    case 201:
        return "Created";
    case 202:
        return "Accepted";
    case 203:
        return "Non-Authoritative Information";
    case 204:
        return "No Content";
    case 205:
        return "Reset Content";
    case 206:
        return"Partial Content";
    case 300:
        return"Multiple Choices";
    case 301:
        return"Moved Permanently";
    case 302:
        return"Moved Temporarily";
    case 303:
        return"See Other";
    case 304:
        return"Not Modified";
    case 305:
        return"Use Proxy";
    case 400:
        return"Bad Request";
    case 401:
        return"Unauthorized";
    case 402:
        return"Payment Required";
    case 403:
        return"Forbidden";
    case 404:
        return"Not Found";
    case 405:
        return"Method Not Allowed";
    case 406:
        return"Not Acceptable";
    case 407:
        return"Proxy Authentication Required";
    case 408:
        return"Request Time-out";
    case 409:
        return"Conflict";
    case 410:
        return"Gone";
    case 411:
        return"Length Required";
    case 412:
        return"Precondition Failed";
    case 413:
        return"Request Entity Too Large";
    case 414:
        return"Request-URI Too Large";
    case 415:
        return"Unsupported Media Type";
    case 500:
        return"Internal Server Error";
    case 501:
        return"Not Implemented";
    case 502:
        return"Bad Gateway";
    case 503:
        return"Service Unavailable";
    case 504:
        return"Gateway Time-out";
    case 505:
        return"HTTP Version not supporte";
    default:
        return "Unknown";
    }
};

class IncomingMessageImpl : public IncomingMessage {
    ::http_parser parser;
    ::http_parser_settings parserSettings;
    std::string mCurrHeader;
    std::string mCurrValue;
    bool inHeaderValueState = false;
    bool onMessageCompleteCalled = false;
public:
    IncomingMessageImpl(Socket<boost::asio::ip::tcp>& socket,
                        HttpServer& server);
private:
public:
    void parseRequest() {
        socket.on(data, [this](const char* d, size_t s) {
            ::http_parser_execute(&parser, &parserSettings, d, s);
            if (onMessageCompleteCalled && parser.upgrade == 1) {
                handleUpgrade(parser, d);
            }
        });
    }
    void onUrl(const char* str, size_t len) {
        mUrl.append(str, str + len);
    }

    void onHeaderField(const char* str, size_t len)
    {
        if (inHeaderValueState) {
            mHeaders.emplace(mCurrHeader, mCurrValue);
            mCurrHeader.clear();
            mCurrValue.clear();
            inHeaderValueState = false;
        }
        mCurrHeader.append(str, str + len);
    }

    void onHeaderValue(const char* str, size_t len)
    {
        inHeaderValueState = true;
        mCurrValue.append(str, len);
    }

    void onHeadersComplete()
    {
        if (mCurrHeader.empty()) return;
        mHeaders.emplace(mCurrHeader, mCurrValue);
        mKeepAlive = ::http_should_keep_alive(&parser) != 0;
        mHttpMajor = parser.http_major;
        mHttpMinor = parser.http_minor;
        mMethod = ::http_method_str(static_cast<::http_method>(parser.method));
        IncomingMessage::onMessageBegin();
    }

    void onMessageBegin()
    {
    }

    void onMessageComplete()
    {
        mKeepAlive = ::http_should_keep_alive(&parser) != 0;
        onMessageCompleteCalled = true;
    }

    void onBody(const char* str, size_t len)
    {
        fireEvent(data, str, len);
    }
};

// these functions are just used for the http_parser which
// can only call static functions. It will call back to the
// IncomingMessage

int on_header_field(::http_parser* parser, const char* at, size_t length) {
    auto msg = reinterpret_cast<IncomingMessageImpl*>(parser->data);
    msg->onHeaderField(at, length);
    return 0;
}

int on_header_value(::http_parser* parser, const char* str, size_t len) {
    auto msg = reinterpret_cast<IncomingMessageImpl*>(parser->data);
    msg->onHeaderValue(str, len);
    return 0;
}

int on_headers_complete(::http_parser* parser)
{
    auto msg = reinterpret_cast<IncomingMessageImpl*>(parser->data);
    msg->onHeadersComplete();
    return 0;
}

int on_message_begin(::http_parser* parser) {
    auto msg = reinterpret_cast<IncomingMessageImpl*>(parser->data);
    msg->onMessageBegin();
    return 0;
}

int on_message_complete(::http_parser* parser) {
    auto msg = reinterpret_cast<IncomingMessageImpl*>(parser->data);
    msg->onMessageComplete();
    return 0;
}

int on_url(::http_parser* parser, const char* at, size_t length) {
    auto msg = reinterpret_cast<IncomingMessageImpl*>(parser->data);
    msg->onUrl(at, length);
    return 0;
}

int on_body(::http_parser* parser, const char* str, size_t length) {
    auto msg = reinterpret_cast<IncomingMessageImpl*>(parser->data);
    msg->onBody(str, length);
    return 0;
}

IncomingMessageImpl::IncomingMessageImpl(Socket<ip::tcp>& socket, HttpServer& server)
        : IncomingMessage(socket, server)
    {
        socket.on(close, [this](bool){
            delete this;
        });
        ::http_parser_init(&parser, ::HTTP_REQUEST);
        parser.data = this;
        parserSettings.on_url = &on_url;
        parserSettings.on_header_field = &on_header_field;
        parserSettings.on_header_value = &on_header_value;
        parserSettings.on_headers_complete = &on_headers_complete;
        parserSettings.on_message_begin = &on_message_begin;
        parserSettings.on_message_complete = &on_message_complete;
    }

}

IncomingMessage::~IncomingMessage() {}

void IncomingMessage::onMessageBegin()
{
    if (recycledResponse == nullptr) {
        recycledResponse = new HttpServerResponse(*this);
    } else {
        recycledResponse->reset();
    }
    server.messageBegin(this, recycledResponse);
}

void IncomingMessage::handleUpgrade(const ::http_parser& parser, const std::string& buffer) {
    std::string initialData(buffer.begin() + parser.nread, buffer.end());
    clearListeners(data);
    if (recycledResponse == nullptr) {
        recycledResponse = new HttpServerResponse(*this);
    } else {
        recycledResponse->reset();
    }
    if (!fireEvent(upgrade, *this, *recycledResponse, initialData)) {
        socket.close();
    }
}

HttpServer::HttpServer()
{
    server.on(connection, [this](auto& socket) { openedConnection(socket); });
} 

void HttpServer::listen(const std::string& port, const std::string& host)
{
    server.listen(port, host);
}

void HttpServer::openedConnection(Socket<boost::asio::ip::tcp>& socket) {
    auto msg = new IncomingMessageImpl(socket, *this);
    msg->parseRequest();
}

void HttpServer::messageBegin(IncomingMessage* req, HttpServerResponse* resp) {
    fireEvent(request, *req, *resp);
}

HttpServerResponse::HttpServerResponse(IncomingMessage& incomingMessage)
    : incomingMessage(incomingMessage)
    , sendCloseHeader(!incomingMessage.mKeepAlive)
{
    incomingMessage.socket.on(close, [this](bool){
        delete this;
    });
}

void HttpServerResponse::reset()
{
    sendCloseHeader = !incomingMessage.mKeepAlive;
    mHeadersSent = false;
    statusCode = 200;
    sendDate = true;
    statusMessage.clear();
}

void HttpServerResponse::prepareSend()
{
    if (mHeadersSent) return;
    // here we need to send the headers
    std::stringstream ss;
    ss << "HTTP/";
    if (incomingMessage.mHttpMajor < 2) {
        ss << incomingMessage.httpVersion() << ' ';
    } else {
        ss << "1.1 ";
    }
    ss << statusCode
       << ' '
       << (statusMessage.empty() ? defaultStatusMessage(statusCode) : statusMessage)
       << "\r\n";
    if (sendDate) {
        ss << "Date: ";
        getHttpDate(ss);
        ss << "\r\n";
    }
    if (!mHeaders.count("Server"))
        ss << "Server: Nodecxx/0.1\r\n";
    if (sendCloseHeader) {
        ss << "Connection: close\r\n";
    }
    if (mContentLength) {
        ss << "Content-Length: " << mContentLength << "\r\n";
    }
    for (auto& p : mHeaders) {
        ss << p.first << ": " << p.second << "\r\n";
    }
    ss << "\r\n";
    const auto& s = ss.str();
    incomingMessage.socket.write(s);
}


} // namespace nodecxx

