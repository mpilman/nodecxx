#pragma once
#include <string>

namespace nodecxx {

struct StringSlice {
    const char*& begin;
    const char*& end;
public:
    StringSlice(const char*& begin, const char*& end)
        : begin(begin)
        , end(end)
    {}
    StringSlice& operator= (const std::string& str) {
        begin = str.data();
        end = str.data() + str.size();
        return *this;
    }
    std::string str() const {
        return std::string(begin, end);
    }
};

class url {
public:
    static std::unique_ptr<url> parse(const char* href);
    virtual ~url();
public: // access
    virtual std::string href() const = 0;
    virtual StringSlice scheme() = 0;
    virtual StringSlice host() = 0;
};

} // namespace

