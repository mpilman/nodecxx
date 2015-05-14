#include "fs.hpp"
#include <core.hpp>

using namespace boost::asio;

namespace nodecxx {

namespace impl {

io_service::id FileService::id;

FileService::~FileService() {}

auto FileService::native_handle(File& file) -> native_handle_type {
    return file.native_handle();
}

boost::system::error_code FileService::open(File& file, const char* filename, const char* mode) {
    auto res = ::fopen(filename, mode);
    if (res) {
        file.file = res;
        boost::system::error_code();
    }
    return boost::system::error_code(errno, boost::system::system_category());
}

boost::system::error_code FileService::close(File& file) {
    if (::fclose(file.file)) {
        return currerror();
    }
    file.file = nullptr;
    return noerror();
}

void FileService::shutdown_service() {
}

} // namespace impl

} // namespace nodecxx
