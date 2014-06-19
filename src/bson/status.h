#pragma once

#include <string>
#include "errorcodes.h"

namespace _bson { 

    class Status { 
    public:

        Status(ErrorCodes e, const std::string& str) : _code(e), s(str) { }

        static Status OK();
        bool isOK() const;

        bool operator==(const Status&) const;
        bool operator!=(const Status&) const;

        ErrorCodes code() const { return _code; }

        std::string codeString() const { return s;  }
        unsigned reason() const { return 0;  }
    private:
        ErrorCodes _code;
        std::string s;
    };

}
