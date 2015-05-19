#include "url.hpp"
#include <uriparser/Uri.h>

namespace nodecxx {

namespace {

class uri_impl : public url {
    ::UriUriA uri;
    std::string mHref;
public:
    uri_impl(const char* href)
        : mHref(href)
    {
        ::UriParserStateA state;

        state.uri = &uri;
        if (::uriParseUriA(&state, this->mHref.c_str()) != URI_SUCCESS) {
        }
    }
    virtual ~uri_impl() {
        ::uriFreeUriMembersA(&uri);
    }
public: // field access
    std::string href() const override {
        return mHref;
    }
    StringSlice scheme() override {
        return StringSlice(uri.scheme.first, uri.scheme.afterLast);
    }
    StringSlice host() override {
        return StringSlice(uri.hostText.first, uri.hostText.afterLast);
    }
};

} // anonymous namespace


url::~url() {
}

std::unique_ptr<url> url::parse(const char* href) {
    return std::unique_ptr<url>(new uri_impl(href));
}

} // namespace nodecxx
