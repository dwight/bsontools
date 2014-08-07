#pragma once

#include <iostream>
#include <fstream>

/** read BSON documents from stdin.  handles binary mode and such for the class user. */
class StdinDocReader {
public:
    void go() { 
        iter(); 
        finished();
    }
    unsigned nDocs() { return docnumber; }
private:
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
    istream& stream;
public:
    StdinDocReader(istream& s = std::cin) : stream(s) {
        if (&s == &std::cin) {
            _setmode(_fileno(stdin), _O_BINARY);
        }
    }
protected:
    virtual void finished() { }
    bool done = false; // subclass can set to terminate processing early
    virtual bool doc(bsonobj&) = 0;
    int iter()
    {
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

class DocReader {
public:
    unsigned nDocs() { return docnumber; }
private:
    const unsigned MaxSize = 128 * 1024 * 1024 + 1;
    unsigned long long bytes = 0;
    unsigned docnumber = 0;
    int have = 0;
    char * buf = new char[MaxSize];
    char * p = buf;
    bool tryReading() {
        static char *lastbad = 0;
        obj = bsonobj(p);
        int sz = obj.objsize();
        if (have < sz) {
            if (lastbad == p) {
                cerr << "error: bad or truncated data in input stream around doc number: " << docnumber << endl;
                exit(2);
            }
            return false;
        }
        lastbad = 0;
        p += sz;
        have -= sz;
        bytes += sz;
        docnumber++;
        return true;
    }
    istream& stream;
public:
    DocReader(istream& s) : stream(s) {
        if (&s == &std::cin) {
            _setmode(_fileno(stdin), _O_BINARY);
        }
    }

    bsonobj obj;

    // retval: 0=ok, -1=eof, >0=error
    // read obj above if ok
    int next()
    {
        while (1) {
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
                if (stream.eof()) {
                    if (have) {
                        cerr << "warning: " << have << " bytes at end of file past the last bson document" << endl;
                        return 2;
                    }
                    return -1;
                }
                stream.read(buf + have, MaxSize - have);
                unsigned n = (unsigned)stream.gcount();
                have += n;
            }
            else {
                break;
            }
        }
        return 0;
    }
};

