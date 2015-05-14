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
#include <events.hpp>
#include <core.hpp>

namespace nodecxx {


template<class Protocol>
class Socket : public EmittingEvents<close_t, data_t, error_t, drain_t> {
    boost::asio::basic_stream_socket<Protocol> socket;
    std::vector<char> buffer;
    std::queue<std::pair<std::vector<char>, bool>> sendBuffer;
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
        sendBuffer.emplace(std::forward<B>(data), false);
        if (sendBuffer.size() == 1) do_send();
    }
    template<class B>
    void end(B&& data) {
        sendBuffer.emplace(std::forward<B>(data), true);
        if (sendBuffer.size() == 1) do_send();
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
            fireEvent(data, buffer);
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

