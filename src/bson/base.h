#pragma once

#define NOINLINE_DECL

namespace _bson {

    class MsgAssertionException {
    public:
        MsgAssertionException(unsigned, std::string);
    };

    class StringData;

    void massert(unsigned, const char *, bool);
    void uassert(unsigned, const char *, bool);
    void msgasserted(unsigned, StringData);
    void verify(bool);

}
