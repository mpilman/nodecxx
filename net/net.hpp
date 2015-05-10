#pragma once
#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <queue>
#include <cassert>
#include <atomic>
#include <iostream>

#include <boost/asio.hpp>

#include "events.hpp"

namespace nodecxx {

namespace core {
extern boost::asio::io_service service;
}

template<class Protocol>
class Socket {
    boost::asio::basic_stream_socket<Protocol> socket;
    std::vector<char> buffer;
    std::queue<std::pair<std::vector<char>, bool>> sendBuffer;
private: // event listeners
    std::vector<std::function<void(const boost::system::error_code&)>> errorListeners;
    std::vector<std::function<void()>> drainListeners;
    std::vector<std::function<void(const std::vector<char>&)>> readListeners;
    std::vector<std::function<void(bool)>> closeListeners;
public:
    Socket() : socket(core::service) {}
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
        sendBuffer.emplace(std::forward<B>(data), false);
        if (sendBuffer.size() == 1) do_send();
    }
    template<class B>
    void end(B&& data) {
        sendBuffer.emplace(std::forward<B>(data), true);
        if (sendBuffer.size() == 1) do_send();
    }
public: // events
    template<class C>
    void on(error_t, C&& callback) {
        errorListeners.emplace_back(std::forward<C>(callback));
    }
    template<class C>
    void on(drain_t, C&& callback)
    {
        drainListeners.emplace_back(std::forward<C>(callback));
    }
    template<class C>
    void on(data_t, C&& callback)
    {
        readListeners.emplace_back(std::forward<C>(callback));
    }
    template<class C>
    void on(close_t, C&& callback)
    {
        closeListeners.emplace_back(std::forward<C>(callback));
    }
public:
    void do_read()
    {
        if (!socket.is_open()) return;
        buffer.resize(1024);
        socket.async_read_some(boost::asio::buffer(buffer),
                [this](const boost::system::error_code& ec, size_t bt) {
            if (check_error(ec)) return;
            buffer.resize(bt);
            for (auto& f : readListeners) {
                f(buffer);
            }
            do_read();
        });
    }
private:
    bool check_error(const boost::system::error_code& ec) {
        if (!ec) return false;
        for (auto& f : errorListeners) {
            f(ec);
        }
        if (!socket.is_open()) close();
        return true;
    }
    void do_send()
    {
        assert(sendBuffer.size());
        boost::asio::async_write(socket, boost::asio::buffer(sendBuffer.front().first), [this](const boost::system::error_code& ec, size_t) {
            if (check_error(ec)) return;
            if (sendBuffer.front().second) {
                close();
            } else {
                // TODO: This code needs some synchronization!
                sendBuffer.pop();
                if (sendBuffer.size()) do_send();
                else {
                    for (auto& f : drainListeners) {
                        f();
                    }
                }
            }
        });
    }
};

template<class Protocol>
class Server {
    std::function<void(Socket<Protocol>&)> callback;
    std::vector<typename Protocol::acceptor> acceptors;
public:
    template<class Callback>
    Server(Callback&& callback) : callback(std::forward<Callback>(callback)) {}
    void listen(const std::string& port, const std::string& host);
private:
    void do_accept(typename Protocol::acceptor& acceptor);
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
    auto resolver = std::make_shared<typename Protocol::resolver>(core::service);
    auto strand = std::make_shared<boost::asio::io_service::strand>(core::service);
    resolver->async_resolve(typename Protocol::resolver::query(host, port),
        strand->wrap([this, resolver](const boost::system::error_code& ec, typename Protocol::resolver::iterator iterator) {
            typename Protocol::resolver::iterator end;
            for (; iterator != end; ++iterator) {
                acceptors.emplace_back(core::service, *iterator);
                auto& myAcceptor = acceptors.back();
                do_accept(myAcceptor);
            }
        }));
}

template<class Protocol>
void Server<Protocol>::do_accept(typename Protocol::acceptor& acceptor)
{
    auto sock = new Socket<Protocol>();
    acceptor.async_accept(sock->native(), [this, sock, &acceptor](const boost::system::error_code& ec) {
        do_accept(acceptor);
        sock->do_read();
        callback(*sock);
    });
}

template<class Protocol>
void Socket<Protocol>::close()
{
    bool hadError = socket.is_open();
    if (!hadError)
        socket.close();
    for (auto& f : closeListeners) {
        f(hadError);
    }
    delete this;
}

} // namespace nodecxx

