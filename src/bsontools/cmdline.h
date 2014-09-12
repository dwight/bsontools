#pragma once

#include <set>
#include <vector>
#include "../../../bson-cxx/src/bson/parse_number.h"

using namespace _bson;
using namespace std;

class option {
public:
    std::string opt;
    std::string value;
    int v;
};

class cmdline {
    int c;
    char **v;
    std::vector<option> options; // -dash or --dash things
public:
    std::vector<string> items;   // regular items on cmd line no dash

    cmdline(int argc, char* argv[], set<string> *optionsWithParameter = 0) : c(argc), v(argv) {
        for (int i = 1; i < argc; i++) {
            const char *p = argv[i];
            if (*p == '-') {
                option m;
                // look for an option value after a dash item, e.g. 
                //   -n 10 
                // becomes a single 'item'.
                p++;
                if (*p == '-') p++;
                m.opt = p;
                int j = i + 1;
                if (j < argc && argv[j][0] != '-' && 
                    (isdigit(argv[j][0]) || (optionsWithParameter && optionsWithParameter->count(p)))
                    ) {
                    m.value = argv[j];
                    i++;
                }
                options.push_back(m);
            }
            else {
                items.push_back(argv[i]);
            }
        }
    }

    // get regular non-dashed arguments
    std::string first() {
        return items.empty() ? "" : *items.begin();
    }

    std::string second() {
        return items.size() >= 2 ? items[1] : "";
    }

    // get value from cmdline 
    //   --PARMNAME VALUE
    string getStr(string parmName) {
        int res = -1;
        for (unsigned i = 0; i < options.size(); i++) {
            if (options[i].opt == parmName) {
                return options[i].value;
            }
        }
        return "";
    }

    // e.g. get '10' from cmdline:
    //   -n 10
    // getNum("n")
    unsigned getNum(string parmName) {
        int res = -1;
        parseNumberFromString(getStr(parmName), &res);
        return res;
    }

    // look for --someoption
    bool got(const char *s) {
        for (unsigned i = 0; i < options.size(); i++) {
            if (options[i].opt == s)
                return true;
        }
        return false;
    }
    
    bool help() {
        return got("help") || got("h");
    }
};
