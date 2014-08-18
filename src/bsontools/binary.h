#pragma once

#include "fcntl.h"
#if defined(_WIN32)
#include "io.h"
#endif
#include <string>

inline void binaryStdIn() {
#if !defined(_WIN32)
    freopen(NULL, "rb", stdin);
#else
    _setmode(_fileno(stdin), _O_BINARY);
#endif
}

inline void binaryStdOut() {
#if !defined(_WIN32)
    freopen(NULL, "wb", stdout);
#else
    _setmode(_fileno(stdout), _O_BINARY);
#endif  
}

class bsontool_error : public std::exception {
    std::string msg;
public:
    virtual ~bsontool_error() throw() {}
    bsontool_error(const char *p) : msg(p) { }
    virtual const char* what() const throw() { return msg.c_str(); }
};

