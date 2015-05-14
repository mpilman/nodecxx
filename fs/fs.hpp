#pragma once
#include <cstdio>
#include <cerrno>
#include <thread>
#include <tuple>
#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>

namespace nodecxx {
namespace impl {

boost::system::error_code currerror() {
    return boost::system::error_code(errno, boost::system::system_category());
}

boost::system::error_code noerror() {
    return boost::system::error_code();
}

class File;

class FileService : public boost::asio::io_service::service {
    boost::asio::io_service threadPoolService;
    boost::asio::io_service::work work;
    std::thread workerThread;
public:
    using implementation_type = File;
    using native_handle_type = FILE*;
public:
    static boost::asio::io_service::id id;
    explicit FileService(boost::asio::io_service& ios)
        : boost::asio::io_service::service(ios)
        , work(threadPoolService)
    {
        workerThread = std::thread([this](){ threadPoolService.run(); });
    }
    virtual ~FileService();
public: // interface
    native_handle_type native_handle(File& f);

    boost::system::error_code open(File& file, const char* filename, const char* mode);

    template<class Callback>
    void async_open(File& file, const char* filename, const char* mode, Callback&& callback);
    boost::system::error_code close(File& file);
public: // Random Access Handle Implementation
    template<class Buffer>
    size_t read_some_at(File& file, uint64_t offset, Buffer& buffer, boost::system::error_code& ec);
    template<class Buffer, class RequestHandler>
    void async_read_some_at(File& file, uint64_t offset, Buffer buffer, RequestHandler handler);

    template<class Buffer>
    size_t write_some_at(File& file, uint64_t offset, Buffer& buffer, boost::system::error_code& ec);
    template<class WriteHandler, class Buffer>
    void async_write_some(File& file, uint64_t offset, Buffer buffer, WriteHandler handler);

    template<class Buffer>
    size_t read_some(File& file, Buffer buffer, boost::system::error_code& ec);
    template<class ReadHandler, class Buffer>
    void async_read_some(File& file, Buffer buffer, ReadHandler handler);

    template<class Buffer>
    size_t write_some(File& file, Buffer buffer, boost::system::error_code& ec);
    template<class ReadHandler, class Buffer>
    void async_write_some(File& file, Buffer buffer, ReadHandler handler);
private:
    virtual void shutdown_service();
};

class File {
    friend class FileService;
    FILE* file = nullptr;
    boost::asio::io_service& io_service;
    FileService& service;
public:
    File(boost::asio::io_service& service)
        : io_service(service)
        , service(boost::asio::use_service<FileService>(service))
    {}
    boost::asio::io_service& get_io_service() {
        return io_service;
    }
    bool is_open() const {
        return file != nullptr;
    }
    boost::system::error_code close(boost::system::error_code&) {
        int err = ::fclose(file);
        return boost::system::error_code(errno, boost::system::system_category());
    }
    FILE* native_handle() {
        return file;
    }

    boost::system::error_code open(const char* filename, const char* mode) {
        return service.open(*this, filename, mode);
    }

    template<class Callback>
    void async_open(const char* filename, const char* mode, Callback&& callback);

public: // Random Access Handle Implementation
    template<class Buffer>
    size_t read_some_at(uint64_t offset, Buffer& buffer, boost::system::error_code& ec) {
        service.read_some_at(*this, offset, buffer, ec);
    }
    template<class Buffer, class RequestHandler>
    void async_read_some_at(uint64_t offset, Buffer buffer, RequestHandler handler) {
        service.async_read_some_at(*this, offset, buffer, handler);
    }

    template<class Buffer>
    size_t write_some_at(uint64_t offset, Buffer& buffer, boost::system::error_code& ec) {
        service.write_some_at(*this, offset, buffer, ec);
    }
    template<class WriteHandler, class Buffer>
    void async_write_some(uint64_t offset, Buffer buffer, WriteHandler handler) {
        service.async_read_some_at(*this, offset, buffer, handler);
    }

    template<class Buffer>
    size_t read_some(Buffer buffer, boost::system::error_code& ec) {
        return service.read_some(*this, buffer, ec);
    }
    template<class ReadHandler, class Buffer>
    void async_read_some(Buffer buffer, ReadHandler handler) {
        service.async_read_some(*this, buffer, handler);
    }

    template<class Buffer>
    size_t write_some(Buffer buffer, boost::system::error_code& ec) {
        service.write_some(*this, buffer, ec);
    }
    template<class ReadHandler, class Buffer>
    void async_write_some(Buffer buffer, ReadHandler handler) {
        service.async_write_some(*this, buffer, handler);
    }
};

template<class Callback>
void File::async_open(const char* filename, const char* mode, Callback&& callback) {
    service.async_open(*this, filename, mode, callback);
}

template<class Callback>
void FileService::async_open(File& file,
    const char* filename,
    const char* mode,
    Callback&& callback) {
    std::function<void(const boost::system::error_code&)> f(std::forward<Callback>(callback));
    auto args = new std::tuple<std::string, std::string, std::function<void(const boost::system::error_code&)>>(std::string(filename), std::string(mode), std::move(f));
    threadPoolService.post([this, args, &file](){
        auto ec = open(file, std::get<0>(*args).c_str(), std::get<1>(*args).c_str());
        std::get<2>(*args)(ec);
        delete args;
    });
}

template<class Buffer>
size_t FileService::read_some_at(File& file, uint64_t offset, Buffer& buffer, boost::system::error_code& ec)
{
    if (::fseeko(file.file, offset, SEEK_SET)) {
        ec = currerror();
        return 0;
    }
    auto from = buffer.begin();
    auto to = buffer.end();
    auto maxSize = std::distance(from, to);
    ec = noerror();
    return ::fread(from, maxSize, sizeof(typename Buffer::value_type), maxSize, file.file);
}

template<class Buffer, class RequestHandler>
void FileService::async_read_some_at(File& file, uint64_t offset, Buffer buffer, RequestHandler handler)
{
    threadPoolService.post([&file, offset, buffer, handler, this](){
        boost::system::error_code ec;
        auto res = read_some_at(file, offset, buffer, ec);
        handler(ec, res);
    });
}

template<class Buffer>
size_t FileService::write_some_at(File& file, uint64_t offset, Buffer& buffer, boost::system::error_code& ec) {
    if (::fseek(file.file, offset, SEEK_SET)) {
        ec = currerror();
        return 0;
    }
    auto from = buffer.begin();
    auto to = buffer.end();
    auto res = std::distance(from, to);
    if (::fwrite(from, sizeof(typename Buffer::value_type), res, file.file) != res * sizeof(typename Buffer::value_type)) {
        ec = currerror();
        return 0;
    }
    ec = noerror();
    return res * sizeof(typename Buffer::value_type);
}
template<class WriteHandler, class Buffer>
void FileService::async_write_some(File& file, uint64_t offset, Buffer buffer, WriteHandler handler) {
    threadPoolService.post([this, &file, offset, buffer, handler]() {
        boost::system::error_code ec;
        auto res = write_some_at(file, offset, buffer, ec);
        handler(ec, res);
    });
}

template<class Buffer>
size_t FileService::read_some(File& file, Buffer buffer, boost::system::error_code& ec) {
    auto res = ::fread(buffer.begin(), sizeof(typename Buffer::value_type), std::distance(buffer.begin(), buffer.end()), file.file);
    if (res == 0) {
        ec = currerror();
    }
    return res;
}
template<class ReadHandler, class Buffer>
void FileService::async_read_some(File& file, Buffer buffer, ReadHandler handler) {
    threadPoolService.post([this, &file, buffer, handler](){
        boost::system::error_code ec;
        auto res = read_some(file, buffer, ec);
        handler(ec, res);
    });
}

template<class Buffer>
size_t FileService::write_some(File& file, Buffer buffer, boost::system::error_code& ec) {
    auto res = ::fwrite(buffer.begin(), sizeof(typename Buffer::value_type), std::distance(buffer.begin(), buffer.end()), file.file);
    if (res == 0) {
        ec = currerror();
    }
    return res;
}
template<class ReadHandler, class Buffer>
void FileService::async_write_some(File& file, Buffer buffer, ReadHandler handler) {
    threadPoolService.post([this, &file, buffer, handler](){
        boost::system::error_code ec;
        auto res = write_some(file, buffer, ec);
        handler(ec, res);
    });
}

} // namespace impl


} // namespace nodecxx
