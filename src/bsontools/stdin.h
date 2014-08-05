#pragma once

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
protected:
    virtual void finished() { }
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

