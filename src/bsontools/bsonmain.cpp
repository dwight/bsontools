#include <iostream>
#include <fstream>
#include "io.h"
#include <fcntl.h>
#include "../../../bson-cxx/src/bson/bsonobj.h"
#include "cmdline.h"

using namespace std;
using namespace _bson;

#include "stdin.h"

namespace bsontools {

    class print : public StdinDocReader {
    public:
        virtual bool doc(bsonobj& b) {
            cout << b.toString() << endl;
            return true;
        }
    };

    class count : public StdinDocReader {
    public:
        int largest = 0;
        virtual bool doc(bsonobj& b) {
            if (b.objsize() > largest)
                largest = b.objsize();
            return true;
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
        else if (s == "print") {
            print x;
            x.go();
        }
        else if (s == "count") {
            count x;
            x.go();
            cout << x.nDocs() << '\t' << x.largest << endl;
        }
        else {
            cout << "Error: bad command line option?  Use -h for help.";
            return -1;
        }
        return 0;
    }

}

bool parms(cmdline& c) {
    if (c.help()) {
        cout << "bson - utility for manipulating BSON files.\n\n";
        cout << "BSON (\"Binary JSON\") is a binary representation of JSON documents.\n"; 
        cout << "This program provides simple utility-type manipulations of BSON files.\n";
        cout << "A BSON file (typically with .bson suffix) is simply a series of BSON \n";
        cout << "documents appended contiguously in a file.\n\n";
        cout << "The utility reads BSON documents from stdin. Thus you can do various \n";
        cout << "piping-type operations from the command line if desired.\n";
        cout << "\n";
        cout << "Usage:\n";
        cout << "\n";
        cout << "  bson <verb> <options>\n";
        cout << "  bson -h                     for help\n";
        cout << "\n";
        cout << "Verbs:\n";
        cout << "  head [-n <N>]               output the first N documents. (N=10 default)\n";
        cout << "  count                       output # of documents in file, then size in bytes of largest document\n";
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
        return bsontools::go(c);
    }
    catch (std::exception& e) {
        cerr << "bson util error: ";
        cerr << e.what() << endl;
        return EXIT_FAILURE;
    }
    return 0;
}
