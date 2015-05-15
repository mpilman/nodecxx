#pragma once
#include <string>
#include <memory>
#include <functional>
#include <queue>
#include <cassert>
#include <atomic>
#include <iostream>

#include <boost/asio.hpp>

#include "events.hpp"
#include <events.hpp>
#include <core.hpp>
#include <json>

namespace nodecxx {

template<class T>
struct serializer {
    std::string operator() (const T& obj) const {
        return std::string(obj.begin(), obj.end());
    }
};

template<>
struct serializer<std::string> {
    template<class O>
    std::string operator() (O&& obj) const {
        return std::string(std::forward<O>(obj));
    }
};

template<>
struct serializer<Json> {
    std::string operator() (const Json& json) const {
        return json.dump();
    }
};

template<class Protocol>
class Socket : public EmittingEvents<close_t, data_t, error_t, drain_t> {
    boost::asio::basic_stream_socket<Protocol> socket;
    std::vector<char> buffer;
    std::queue<std::pair<std::string, bool>> sendBuffer;
    bool insideSend = false;
public:
    Socket() : socket(core::service()) {}
public: // functionality
    boost::asio::basic_stream_socket<Protocol>& native() { return socket; }
    void close();
    size_t bufferSize() const {
        size_t res = 0;
        for (const auto& b: sendBuffer) {
            res += b.size();
        }
    }
    template<class B>
    void write(B&& data) {
        serializer<typename std::remove_const<typename std::remove_reference<B>::type>::type> ser;
        sendBuffer.emplace(ser(std::forward<B>(data)), false);
        do_send();
    }
    template<class B>
    void end(B&& data) {
        serializer<typename std::remove_const<typename std::remove_reference<B>::type>::type> ser;
        sendBuffer.emplace(ser(std::forward<B>(data)), true);
        if (sendBuffer.size() == 1) do_send();
    }
public:
    void do_read()
    {
        if (!socket.is_open()) return;
        buffer.resize(1024);
        // TODO: this might not be safe
        socket.async_read_some(boost::asio::buffer(buffer),
                [this](const boost::system::error_code& ec, size_t bt) {
            if (check_error(ec)) return;
            buffer.resize(bt);
            fireEvent(data, buffer.data(), buffer.size());
            do_read();
        });
    }
private:
    bool check_error(const boost::system::error_code& ec) {
        if (!ec) return false;
        fireEvent(error, ec);
        fireEvent(::nodecxx::close, true);
        if (socket.is_open())
            socket.close();
        else
            delete this;
        return true;
    }
    void do_send()
    {
        if (insideSend) return;
        insideSend = true;
        boost::asio::async_write(socket, boost::asio::buffer(sendBuffer.front().first), [this](const boost::system::error_code& ec, size_t) {
            if (check_error(ec)) return;
            if (sendBuffer.front().second) {
                close();
            } else {
                sendBuffer.pop();
                insideSend = false;
                if (sendBuffer.size()) do_send();
                else {
                    fireEvent(drain);
                }
            }
        });
    }
};

struct connection_t {
    constexpr connection_t() {}
};

constexpr connection_t connection;

template<class Protocol>
class Server : public EmittingEvents<error_t> {
    std::function<void(Socket<Protocol>&)> callback;
    std::vector<typename Protocol::acceptor> acceptors;
public:
    template<class Callback>
    Server(Callback&& callback) : callback(std::forward<Callback>(callback)) {}
    Server() {}
    void listen(const std::string& port, const std::string& host);
    template<class Callback>
    void on(connection_t, Callback&& callback)
    {
        this->callback = callback;
    }
private:
    void do_accept(size_t acceptorPos);
};

using TcpServer = Server<boost::asio::ip::tcp>;

namespace server {

template<class Callback>
TcpServer createServer(Callback&& callback) {
    return TcpServer(std::forward<Callback>(callback));
}

}


template<class Protocol>
void Server<Protocol>::listen(const std::string& port, const std::string& host)
{
    auto resolver = std::make_shared<typename Protocol::resolver>(core::service());
    auto strand = std::make_shared<boost::asio::io_service::strand>(core::service());
    resolver->async_resolve(typename Protocol::resolver::query(host, port),
        strand->wrap([this, resolver](const boost::system::error_code& ec, typename Protocol::resolver::iterator iterator) {
            typename Protocol::resolver::iterator end;
            for (; iterator != end; ++iterator) {
                acceptors.emplace_back(core::service(), *iterator);
            }
            for (size_t i = 0; i < acceptors.size(); ++i) {
                do_accept(i);
            }
        }));
}

template<class Protocol>
void Server<Protocol>::do_accept(size_t acceptorPos)
{
    auto sock = new Socket<Protocol>();
    acceptors[acceptorPos].async_accept(sock->native(), [this, sock, acceptorPos](const boost::system::error_code& ec) {
        if (ec) {
            fireEvent(error, ec);
            if (sock->native().is_open())
                sock->close();
            else
                delete sock;
        } else {
            callback(*sock);
            sock->do_read();
            do_accept(acceptorPos);
        }
    });
}

template<class Protocol>
void Socket<Protocol>::close()
{
    bool hadError = socket.is_open();
    if (!hadError)
        socket.close();
    this->fireEvent(::nodecxx::close, hadError);
    delete this;
}

} // namespace nodecxx

