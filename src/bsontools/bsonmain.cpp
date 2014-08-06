#include <iostream>
#include <fstream>
#include "io.h"
#include <fcntl.h>
#include "../../../bson-cxx/src/bson/bsonobj.h"
#include "../../../bson-cxx/src/bson/bsonobjiterator.h"
#include "../../../bson-cxx/src/bson/bsonobjbuilder.h"
#include "cmdline.h"
#include <deque>

using namespace std;
using namespace _bson;

#include "stdin.h"

namespace bsontools {

    bool emitDocNumber = false; 

    class bsonobjholder {
        string buf;
    public:
        bsonobjholder(bsonobj& o) : buf(o.objdata(), o.objsize()) { }
        bsonobj obj() const { return bsonobj(buf.c_str()); }
    };

    void write(bsonobj& o, long long docNumber = -1) {
        if (emitDocNumber && docNumber >= 0)
            cout << docNumber << '\n';
        else
            cout.write(o.objdata(), o.objsize());
    }

    class GrepElem {
    public:
        bool i = false;
        string target;
        bool matched = false;
        void reset() { matched = false;  }
        void operator() (bsonelement& x) {
            string s;
            const char *p;
            if (x.isNumber()) {
                s = x.toString(false);
                p = s.c_str();
            }
            else if (x.type() == _bson::String) {
                p = x.valuestr();
            }
            else {
                return;
            }
            if (strstr(p, target.c_str()) != 0)
                matched = true;
        }
    };

    class PrintElem {
    public:
        void reset() {
            s.reset();
        }
        bool operator() (bsonelement& x) {
            if (x.isNumber()) {
                x.toString(s, false, false);
                s << '\t';
            }
            else if (x.type() == _bson::String) {
                s << x.valuestr() << '\t';
            }
            return true;
        }
        StringBuilder s;
    };

    template<typename T>
    void descend(bsonobj& o, T& f) {
        bsonobjiterator i(o);
        while (i.more()) {
            bsonelement e = i.next();
            if (e.isObject()) {
                descend(e.object(),f);
            }
            else {
                f(e);
            }
        }
    }

    class grep : public StdinDocReader {
    public:
        GrepElem f;
        virtual bool doc(bsonobj& b) {
            descend(b, f);
            if (f.matched) {
                write(b, nDocs());
            }
            f.reset();
            return true;
        }
        grep(string s) { 
            f.target = s;
        }
    };

    class text : public StdinDocReader {
    public:
        PrintElem f;
        virtual bool doc(bsonobj& b) {
            descend(b, f);
            cout << f.s.str() << '\n';
            f.reset();
            return true;
        }
    };

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

    /** THIS IS VERY INEFFICIENT. But wanted to just get the capability in a crude form in place. */
    class tail : public StdinDocReader {
        int n = 0;
        deque<bsonobjholder> last;
        virtual void finished() { 
            for (auto i = last.begin(); i != last.end(); i++) {
                bsonobj o = i->obj();
                write(o);
            }
        }
        virtual bool doc(bsonobj& b) {
            if (++n > N) {
                last.pop_front();
            }
            last.push_back(bsonobjholder(b));
            return true;
        }
    public:
        int N = 10;
        tail() {
            _setmode(_fileno(stdout), _O_BINARY);
        }
    };

    class head : public StdinDocReader {
        int n = 0;
        virtual bool doc(bsonobj& b) {
            write(b, nDocs());
            if (++n >= N)
                done = true;
            return true;
        }
    public:
        int N = 10;
        head() {
            _setmode(_fileno(stdout), _O_BINARY);
        }
    };

    int go(cmdline& c) {
        string s = c.first();
        if (s == "head") {
            head h;
            int x = c.getNum("n");
            if (x > 0) {
                h.N = x;
            }
            h.go();
        }
        else if (s == "tail") {
            tail h;
            int x = c.getNum("n");
            if (x > 0) {
                h.N = x;
            }
            h.go();
        }
        else if (s == "print") {
            print x;
            x.go();
        }
        else if (s == "text") {
            text t;
            t.go();
        }
        else if (s == "grep") {
            grep t(c.items[1]);
            t.go();
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
        cout << "\n";
        cout << "Options:\n";
        cout << "  -h                          for help\n";
        cout << "  -#                          emit document number instead of the document's bson content\n";
        cout << "\n";
        cout << "Verbs:\n";
        cout << "  count                       output # of documents in file, then size in bytes of largest document\n";
        cout << "  head [-n <N>]               output the first N documents. (N=10 default)\n";
        cout << "  tail [-n <N>]\n";
        cout << "  print                       convert bson to json (print json to stdout)\n";
        cout << "  text                        extract text values and output (no fieldnames)\n";
        cout << "  grep <pattern>              text search each document for a simple text pattern. not actual\n";
        cout << "                              regular expressions yet. outputs matched documents.\n";
        cout << "\n";
        return true;
    }
    bsontools::emitDocNumber = c.got("#");
    return false;
}

int main(int argc, char* argv[])
{
    try {
        cmdline c(argc, argv);
        if (parms(c))
            return 0;

        // temp
        if( 0 ){
            bsontools::text t;
            bsonobjbuilder b;
            bsonobj o = b.append("x", 3).append("Y", "abc").obj();
            cout << o.toString() << endl;

            t.doc(o);
        }

        return bsontools::go(c);
    }
    catch (std::exception& e) {
        cerr << "bson util error: ";
        cerr << e.what() << endl;
        return EXIT_FAILURE;
    }
    return 0;
}
