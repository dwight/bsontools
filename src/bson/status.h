#pragma once

#include <string>
#include "errorcodes.h"

namespace _bson { 

    class Status { 
    public:

        Status(ErrorCodes e, const std::string& str) : _code(e), s(str) { }
        Status(ErrorCodes e, const std::string& str,int) : _code(e), s(str) { }

        static Status OK() { return Status(Ok, ""); }
        bool isOK() const { return _code == Ok; }

        bool operator==(const Status& rhs) const {
            return _code == rhs._code;
        }
        bool operator!=(const Status& rhs) const { return _code != rhs._code;  }

        ErrorCodes code() const { return _code; }

        std::string codeString() const { return s;  }
        unsigned reason() const { return 0;  }
    private:
        ErrorCodes _code;
        std::string s;
    };

}
