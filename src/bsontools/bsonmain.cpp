#include <iostream>
#include <fstream>
#include "io.h"
#include <fcntl.h>
#include "../../../bson-cxx/src/bson/bsonobj.h"
#include "cmdline.h"

using namespace std;
using namespace _bson;

class StdinDocReader {
    const unsigned MaxSize = 128 * 1024 * 1024 + 1;
    unsigned long long bytes = 0;
    unsigned docnumber = 0;
    int have = 0;
    char * buf = new char[MaxSize];
    char * p = buf;
    bool tryReading() {
        static char *lastbad = 0;
        bsonobj b(p);
        int sz = b.objsize();
        if (have < sz) {
            if (lastbad == p) {
                cerr << "error: bad or truncated data in input stream around doc number: " << docnumber << endl;
                exit(2);
            }
            return false;
        }
        doc(b);
        lastbad = 0;
        p += sz;
        have -= sz;
        bytes += sz;
        docnumber++;
        return true;
    }
public:
    void go() { iter(); }
protected:
    bool done = false;
    virtual bool doc(bsonobj&) = 0;
    int iter()
    {
        _setmode(_fileno(stdin), _O_BINARY);
        while (!done) {
            if (have < 4 || !tryReading()) {
                // need more data
                if (p != buf) {
                    memmove(buf, p, have);
                    p = buf;
                }
                if (have == MaxSize) {
                    cerr << "error: document too large for this utility? doc number:" << docnumber << endl;
                    return 1;
                }
                if (cin.eof()) {
                    if (have) {
                        cerr << "warning: " << have << " bytes at end of file past the last bson document" << endl;
                        return 2;
                    }
                    break;
                }
                cin.read(buf + have, MaxSize - have);
                unsigned n = (unsigned)cin.gcount();
                have += n;
            }
        }
        return 0;
    }
};

class head : public StdinDocReader {
    int n = 0;
    virtual bool doc(bsonobj& b) {
        cout.write(b.objdata(), b.objsize());
        if (++n >= 10)
            done = true;
        return true;
    }
public:
    head(){
        _setmode(_fileno(stdout), _O_BINARY);
    }
};

int go(cmdline& c) {
    string s = c.first();
    if (s == "head") {
        head h;
        h.go();
    }
    else {
        cout << "Error: bad command line option?  Use -h for help.";
        return -1;
    }
    return 0;
}

bool parms(cmdline& c) {
    if (c.help()) {
        cout << "bson - utilities for manipulating BSON files.\n";
        cout << "\n";
        cout << "BSON (\"Binary JSON\") is a binary representation of JSON documents.\n"; 
        cout << "This program provides simple utility-type manipulations of BSON files.\n";
        cout << "A BSON file (typically with .bson suffix) is simply a series of BSON \n";
        cout << "documents appended contiguously in a file.\n";
        cout << "\n";
        cout << "The utility reads BSON documents from stdin. Thus you can do various \n";
        cout << "piping-type operations from the command line if desired.\n";
        cout << "\n";
        cout << "Usage:\n";
        cout << "\n";
        cout << "bson <verb> <options>\n";
        cout << "bson -h                      for help\n";
        cout << "\n";
        cout << "Verbs:\n";
        cout << "  head [-n <N>]              output the first N documents. (N=10 default)\n";
        cout << "\n";
        return true;
    }
    return false;
}

int main(int argc, char* argv[])
{
    try {
        cmdline c(argc, argv);
        if (parms(c))
            return 0;
        return go(c);
    }
    catch (std::exception& e) {
        cerr << "bson util error: ";
        cerr << e.what() << endl;
        return EXIT_FAILURE;
    }
    return 0;
}
