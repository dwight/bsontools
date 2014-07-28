// bcount.cpp : Defines the entry point for the console application.
//

#include <iostream>
#include <fstream>
#include "io.h"
#include <fcntl.h>
#include "../../../bson-cxx/src/bson/bsonobj.h"
#include "cmdline.h"

using namespace std;
using namespace _bson;

const unsigned MaxSize = 128 * 1024 * 1024 + 1;
unsigned largest = 0;
unsigned long long bytes = 0;
unsigned docnumber = 0;
unsigned have = 0;
char * buf = new char[MaxSize];
char * p = buf;

bool do_one() { 
    static char *lastbad = 0;
    //cout << "do_one " << have << endl;
    bsonobj b(p);
    int sz = b.objsize();
    //cout << "  sz " << sz << endl;
    //    if (!b.valid()) {
    if (have < (unsigned) sz) {
        if (lastbad == p) {
            cerr << "error: bad or truncated data in input stream around doc number: " << docnumber << endl;
            exit(2);
        }
        return false;
    }
    lastbad = 0;
    if (largest < (unsigned) sz) largest = sz;
    p += sz;
    have -= sz;
    bytes += sz;
    docnumber++;
    return true;  
}

int go();

int go()
{
    _setmode(_fileno(stdin), _O_BINARY);

    while (1) {
        //cout << "have: " << have << endl;
        if (have < 4 || !do_one()) {
            // need more data
            if (p != buf) {
                memmove(buf, p, have);
                p = buf;
            }
            if (have == MaxSize) {
                //cout << "H " << have << endl;
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
            unsigned n = (unsigned) cin.gcount();
            //out << "gcount " << n << endl;
            have += n;
        }
        else {
            ;
        }
    }

    cout << "docs:    " << docnumber << endl;
    cout << "largest: " << largest << endl;

	return 0;
}

bool parms(cmdline& c) {
    if (c.help()) {
        cout << "bcount utility - count BSON documents in input\n";
        cout << "reports number of documents, and largest document's size";
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
        return go();
    }
    catch (std::exception& e) {
        cerr << "bcount error: ";
        cerr << e.what() << endl;
        return EXIT_FAILURE;
    }
    return 0;
 }

