#pragma once

#define NOINLINE_DECL

namespace _bson {

    class MsgAssertionException {
    public:
        MsgAssertionException(unsigned, std::string);
    };

    void uassert(unsigned, const char *, bool);
    void msgasserted(unsigned, const char *);
    void verify(bool);

}
