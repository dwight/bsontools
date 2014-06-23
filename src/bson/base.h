#pragma once

#include <cassert>

#define NOINLINE_DECL

namespace _bson {

    class MsgAssertionException {
    public:
        MsgAssertionException(unsigned, std::string) {}
    };

    inline void massert(unsigned, const char *, bool x) {
        assert(x);
    }
    inline void massert(unsigned, std::string, bool x) {
        assert(x);
    }
    inline void uasserted(unsigned, std::string) { assert(false); }
    inline void uassert(unsigned, const char *, bool x) {
        assert(x);
    }
    inline void uassert(unsigned, std::string, bool x) {
        assert(x);
    }
    inline void msgasserted(unsigned, std::string) { assert(false); }
    inline void verify(bool x) { assert(x); }

}
