#pragma once
#include <events.hpp>
#include <net/net.hpp>
#include <net/events.hpp>
#include "http_parser.h"

#include <functional>
#include <unordered_map>

namespace nodecxx {

class HttpServer;
class IncomingMessage;

class HttpServerResponse : public EmittingEvents<close_t> {
    friend class IncomingMessage;
    IncomingMessage& incomingMessage;
    bool sendCloseHeader;
    bool mHeadersSent = false;
    std::unordered_map<std::string, std::string> mHeaders;
    unsigned mContentLength = 0;
private:
    void reset();
    void prepareSend();
private: // Construction
    HttpServerResponse(IncomingMessage& incomingMessage);
public:
    int statusCode = 200;
    bool sendDate = true;
    std::string statusMessage;
    bool headersSent() const { return mHeadersSent; }
    template<class S>
    void setHeader(const std::string& name, S&& value) {
        if (name == "Content-Length") {
            mContentLength = std::stoul(value);
        }
        mHeaders.emplace(name, std::forward<S>(value));
    }
    bool getHeader(const std::string& name, std::string& result) const {
        auto i = mHeaders.find(name);
        if (i == mHeaders.end()) return false;
        result = i->second;
        return true;
    }
    bool removeHeader(const std::string& name) {
        auto i = mHeaders.find(name);
        if (i == mHeaders.end()) return false;
        mHeaders.erase(i);
        return true;
    }
    template<class B>
    void write(B&& b);
    template<class B>
    void end(B&& b);
};

struct upgrade_t {
    using function_type = std::function<void(IncomingMessage&, HttpServerResponse&, const std::vector<char>&)>;
    constexpr upgrade_t() {}
};

constexpr upgrade_t upgrade;

class IncomingMessage : public EmittingEvents<close_t, data_t, error_t, upgrade_t> {
    friend class HttpServer;
    friend class HttpServerResponse;
protected:
    Socket<boost::asio::ip::tcp>& socket;
    HttpServer& server;
    std::string mUrl;
    std::string mMethod;
    std::unordered_multimap<std::string, std::string> mHeaders;
    int mHttpMajor = 0;
    int mHttpMinor = 0;
    bool mKeepAlive = true;
    HttpServerResponse* recycledResponse = nullptr;
protected: // internal callbacks
    void onMessageBegin();
    void handleUpgrade(const ::http_parser& parser, const std::vector<char>& buffer);
protected: // construction
    IncomingMessage(Socket<boost::asio::ip::tcp>& socket, HttpServer& server)
        : socket(socket)
        , server(server)
    {}
    virtual ~IncomingMessage();
public: // Access
    const std::string& url() const { return mUrl; }
    std::string httpVersion() const {
        return std::to_string(mHttpMajor) + '.' + std::to_string(mHttpMinor);
    }
    const std::unordered_multimap<std::string, std::string>& headers() const {
        return mHeaders;
    }
    const std::string& method() const {
        return mMethod;
    }
};

struct request_t {
    using function_type = std::function<void(IncomingMessage&, HttpServerResponse&)>;
    constexpr request_t() {}
};

constexpr request_t request;

class HttpServer : public EmittingEvents<request_t> {
    TcpServer server;
    friend class IncomingMessage;
public:
    HttpServer();
    void listen(const std::string& port, const std::string& host);
private:
    void openedConnection(Socket<boost::asio::ip::tcp>& socket);
    void messageBegin(IncomingMessage* req, HttpServerResponse* resp);
};


template<class B>
void HttpServerResponse::write(B&& b)
{
    if (!mHeadersSent)
        prepareSend();
    incomingMessage.socket.write(b);
}

template<class B>
void HttpServerResponse::end(B&& b)
{
    if (!mHeadersSent) {
        if (mContentLength == 0) {
            mContentLength = b.size();
        }
        prepareSend();
    }
    incomingMessage.socket.write(b);
}

} // namespace nodecxx
