#pragma once

#include <vector>
#include "../../../bson-cxx/src/bson/parse_number.h"

using namespace _bson;
using namespace std;

class item {
public:
    std::string option;
    std::string value;
    int v;
};

class cmdline {
    int c;
    char **v;
    std::vector<item> items;
public:
    cmdline(int argc, char* argv[]) : c(argc), v(argv) {
        for (int i = 1; i < argc; i++) {
            item m;
            const char *p = argv[i];
            if (*p == '-') {
                // look for an option value after a dash item, e.g. 
                //   -n 10 
                // becomes a single 'item'.
                p++;
                if (*p == '-') p++;
                m.option = p;
                int j = i + 1;
                if (j < argc && argv[j][0] != '-') {
                    m.value = argv[j];
                    i++;
                }
            }
            else {
                m.option = argv[i];
                m.v = 0;
            }
            items.push_back(m);
        }
    }

    std::string first() { 
        return items.empty() ? "" : items.begin()->option;
    }

    unsigned getNum(string parmName) {
        int res = -1;
        for (unsigned i = 0; i < items.size(); i++) {
            if (items[i].option == parmName) {
                parseNumberFromString(items[i].value, &res);
            }
        }
        return res;
    }

    bool got(const char *s) {
        for (int i = 1; i < c; i++) {
            const char *p = v[i];
            if (strcmp(p, s) == 0)
                return true;
            if (*p == '-') p++;
            if (*p == '-') p++;
            if (strcmp(p, s) == 0)
                return true;
        }
        return false;
    }

    bool help() {
        return got("--help") || got("-h");
    }
};
