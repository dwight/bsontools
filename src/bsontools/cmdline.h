
class item {
public:
    string option;
    string value;
    int v;
};

class cmdline {
    int c;
    char **v;
    vector<item> items;
public:
    cmdline(int argc, char* argv[]) : c(argc), v(argv) {
        for (int i = 1; i < argc; i++) {

        }
    }

    bool got(const char *s) {
        for (auto i = items.begin(); i != i.end(); i++) {

        }
        for (int i = 1; i < c; i++)
            if (strcmp(v[i], s) == 0)
                return true;
        return false;
    }

    bool help() {
        return got("--help") || got("-h");
    }
};
