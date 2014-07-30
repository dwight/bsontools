#pragma once

#include <vector>

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

        }
    }

    bool got(const char *s) {
        for (int i = 1; i < c; i++) {
            const char *p = v[i];
            if (*p == '-') p++;
            if (*p == '-') p++;
            if (strcmp(v[i], s) == 0)
                return true;
        }
        return false;
    }

    bool help() {
        return got("--help") || got("-h");
    }
};
