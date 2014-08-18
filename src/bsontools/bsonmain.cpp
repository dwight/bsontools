#include <iostream>
#include <fstream>
#include "binary.h"
#include "../../../bson-cxx/src/bson/bsonobj.h"
#include "../../../bson-cxx/src/bson/bsonobjiterator.h"
#include "../../../bson-cxx/src/bson/bsonobjbuilder.h"
#include "cmdline.h"
#include <deque>

using namespace std;
using namespace _bson;

#include "stdin.h"

void strSplit(vector<string> &line, const string& str);

namespace bsontools {

    bool emitDocNumber = false; // -# option emits doc # instead of BSON content
    bool emitNull = false;

    class bsonobjholder {
        string buf;
    public:
        bsonobjholder(bsonobj& o) : buf(o.objdata(), o.objsize()) { }
        bsonobj obj() const { return bsonobj(buf.c_str()); }
    };

    void write(const bsonobj& o, long long docNumber = -1) {
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
    void descend(const bsonobj& o, T& f) {
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
            binaryStdOut(); 
        }
    };

    class sample : public StdinDocReader {
        virtual bool doc(bsonobj& b) {
            if (nDocs() % N == 0) {
                write(b, nDocs());
            }
            return true;
        }
    public:
        int N = 10;
        sample() {
            binaryStdOut(); 
        }
    };

    class project : public StdinDocReader {
        virtual bool doc(bsonobj& b) {
            set<string> f(fields.begin(), fields.end());
            {
                bsonobjbuilder bldr;
                bsonobjiterator i(b);
                while (i.more()) {
                    bsonelement e = i.next();
                    if (f.count(e.fieldName())) {
                        bldr.append(e);
                    }
                }
                write(bldr.obj(), nDocs());
            }
            return true;
        }
    public:
        vector<string>fields;
        project() { binaryStdOut(); }
    };

    class del : public StdinDocReader {
        virtual bool doc(bsonobj& b) {
            if (!b.hasField(fieldName)) {
                write(b, nDocs());
            }
            else {
                bsonobjbuilder bldr;
                bsonobjiterator i(b);
                while (i.more()) {
                    bsonelement e = i.next();
                    if (fieldName != e.fieldName()) {
                        bldr.append(e);
                    }
                }
                write(bldr.obj(), nDocs());
            }
            return true;
        }
    public:
        string fieldName;
        del() { binaryStdOut(); }
    };

    /** helper class to specialize from */
    class bsonpipe;
    map<string,bsonpipe*> verbs;
    class bsonpipe : public StdinDocReader {
    protected:
        void write(const bsonobj& o) {
            bsontools::write(o, nDocs());
        }
        // things to override
        virtual bool binaryOutput()     { return true; }
        virtual void finished()         { } 
        virtual bool doc(bsonobj&) = 0;
    public:
        virtual void init(cmdline&)     { }
        virtual string help() = 0;
        explicit bsonpipe(string name) {
            assert(!name.empty());
            verbs.insert(pair<string, bsonpipe*>(name, this));
        }
        virtual void go() {
            if (binaryOutput() && !emitDocNumber)
                binaryStdOut();
            StdinDocReader::go();
        }
    };

    class unwind : public bsonpipe {
        virtual string help() {
            return "unwind <fieldname>          unwind array (or object) elements. dot notation ok.";
        }
        virtual bool doc(bsonobj& b) {
            bsonelement e = b.getFieldDotted(fieldName);
            if (e.isObject() ) {
                bsonobjiterator i( e.object() );
                while (i.more()) {
                    bsonobjbuilder bldr;
                    bldr.appendAs(i.next(), fieldName);
                    write(bldr.obj());
                }
            }
            else if (emitNull) {
                bsonobjbuilder bldr;
                bldr.appendNull(fieldName);
                write(bldr.obj());
            }
            else {
                //write(bsonobj());
            }
            return true;
        }
        string fieldName;
        virtual void init(cmdline& c) {
            fieldName = c.second();
        }
    public:
        unwind() : bsonpipe("unwind") {}
    } _unwind;

    class pull : public bsonpipe {
        virtual string help() {
            return "pull <fieldname>            extract a single field as a singleton element in output documents. dot notation ok.";
        }
        virtual bool doc(bsonobj& b) {
            bsonobjbuilder bldr;
            bsonelement e = b.getFieldDotted(fieldName);
            if (!e.eoo())
                bldr.append(e);
            else if (emitNull)
                bldr.appendNull(fieldName);
            write(bldr.obj());
            return true;
        }
        string fieldName;
        virtual void init(cmdline& c) {
            fieldName = c.second();
        }
    public:
        pull() : bsonpipe("pull") {}
    } _pull;

    class demote : public bsonpipe {
        virtual string help() {
            return "demote <fieldname>          put each document inside a field named fieldname, then output";
        }
        virtual bool doc(bsonobj& b) {
            bsonobjbuilder bldr;
            bldr.append(fieldName, b);
            write(bldr.obj());
            return true;
        }
        string fieldName;
        void init(cmdline& c) {
            fieldName = c.second();
        }
    public:
        demote() : bsonpipe("demote") {}
    } _demote;

    void merge(istream& _a, istream& _b) {
        DocReader a(_a);
        DocReader b(_b);
        while (1) {
            bsonobjbuilder builder;
            int r1 = a.next();
            int r2 = b.next();
            if (r1 > 0)
                exit(r1);
            if (r2 > 0)
                exit(r2);
            if (r1 != r2) {
                cerr << "error: different number of documents in two files to merge" << endl;
                exit(5);
            }
            if (r1 < 0)
                break;

//            cout << '\n' << b.obj.toString() << endl;

            // ok...
            builder.appendElements(a.obj);
            builder.appendElementsUnique(b.obj);
            write(builder.obj(), a.nDocs());
        }
    }

    class promote : public StdinDocReader {
        virtual bool doc(bsonobj& b) {
            //cout << b.toString() << endl;
            bsonelement e = b.getFieldDotted(fieldName);
            if (e.isObject()) {
                write(e.object(), nDocs());
            }
            return true;
        }
    public:
        string fieldName;
        promote() { binaryStdOut(); }
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
            binaryStdOut(); 
        }
    };

    int go(cmdline& c) {
        string s = c.first();
        {
            auto i = verbs.find(s);
            if (i != verbs.end()) {
                i->second->init(c);
                i->second->go();
                return 0;
            }
        }

        if (s == "head") {
            head h;
            int x = c.getNum("n");
            if (x > 0) {
                h.N = x;
            }
            h.go();
        }
        else if (s == "project") {
            project x;
            strSplit(x.fields, c.second());
            x.go();
        }
        else if (s == "del") {
            del x;
            x.fieldName = c.second();
            x.go();
        }
        else if (s == "promote") {
            promote x;
            x.fieldName = c.second();
            x.go();
        }
        else if (s == "merge") {
            string fn = c.second();
            ifstream f(fn);
            if (!f.is_open()) {
                cerr << "error couldn't open file: " << fn << endl;
                exit(1);
            }
            merge(cin, f);
        }
        else if (s == "sample") {
            sample s;
            int x = c.getNum("n");
            if (x < 1)
                throw bsontool_error("bad or missing -n argument");
            s.N = x;
            s.go();
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
            grep t(c.second());
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

const char QUOTE = '"';
const char COMMA = ',';

void strSplit(vector<string> &line, const string& str) {
    line.clear();
    if (str.empty())
        return;

    const char *p = str.c_str();
    while (1) {
        StringBuilder s;
        bool quoted = (*p == QUOTE);
        if (quoted) {
            p++;
        }
        while (1) {
            if (*p == QUOTE) {
                if (p[1] == QUOTE) {
                    s << QUOTE;
                    p += 2;
                }
                else {
                    // end of field
                    uassert(1000, "unexpected quote", quoted);
                    p++;
                    break;
                }
            }
            else if (*p == COMMA && !quoted) {
                break;
            }
            else if (*p == 0) {
                if (!quoted) {
                    break;
                }
                else {
                    throw bsontool_error("expected quote");
                }
            }
            else {
                s << *p++;
            }
        }
        line.push_back(s.str());
        if (*p == COMMA) {
            p++;
            continue;
        }
        if (*p == 0) {
            break;
        }
        throw bsontool_error("error: expected comma or end of line (after quote?)");
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
        cout << "  -N                          emit null rather than nothing, when applicable (eg w/pull)\n";
        cout << "\n";
        cout << "Verbs:\n";
        cout << "  count                       output # of documents in file, then size in bytes of largest document\n";
        cout << "  head [-n <N>]               output the first N documents. (N=10 default)\n";
        cout << "  tail [-n <N>]               note: current tail implementation is slow\n";
        cout << "  sample -n <N>               output every Nth document\n";
        cout << "  print                       convert bson to json (print json to stdout)\n";
        cout << "  text                        extract text values and output (no fieldnames)\n";
        cout << "  grep <pattern>              text search each document for a simple text pattern. Not actual\n";
        cout << "                              regular expressions yet. outputs matched documents.  Element values\n";
        cout << "                              are searched for, field names are not.\n";
        cout << "  project <fieldnamelist>     project comma separated list of top level fieldnames to output\n";
        cout << "  del <fieldname>             delete a top level field and then re-output the stream\n";
        cout << "  promote <fieldname>         pull out the subobject specified by fieldname and output it (only).\n";
        cout << "                              if the field is missing or not an object nothing will be output. Dot\n";
        cout << "                              notation is supported.\n";
        cout << "  merge <filename>            merge bson stream stdin with bson file on cmd line.  should be same\n";
        cout << "                              number of documents in each input.\n";
        for (auto i = bsontools::verbs.begin(); i != bsontools::verbs.end(); i++) {
            cout << "  " << i->second->help() << '\n';
        }
        cout << "\n";
        return true;
    }
    bsontools::emitDocNumber = c.got("#");
    bsontools::emitNull = c.got("N");
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
            
            bsonobjbuilder b;
            bsonobj o = b.append("x", 3).append("Y", "abc").obj();
            cout << o.toString() << endl;
            bsonelement e = o.getFieldDotted("y");
            e.isObject();
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
