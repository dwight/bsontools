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
            item m;
            m.option = argv[i];
            m.v = 0;
            items.push_back(m);
        }
    }

    std::string first() { 
        return items.empty() ? "" : items.begin()->option;
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
