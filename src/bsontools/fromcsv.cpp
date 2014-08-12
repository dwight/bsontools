#include <memory>
#include <iostream>
#include <string>
#include "../../../bson-cxx/src/bson/json.h"
#include "../../../bson-cxx/src/bson/bsonobjbuilder.h"
#include "cmdline.h"

using namespace std;
using namespace _bson;

bool header = false;

const char QUOTE = '"';
const char COMMA = ',';

void skipWhite(const char *& p) {
    while (*p == ' ' || *p == '\t')
        p++;
}

bool getl(vector<string> &line) {
    line.clear();
    string str;
    getline(cin, str);
    if (cin.eof())
        return false;

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
                    // span
                    s << '\n';
                    if (cin.eof())
                        break;
                    str.clear();
                    getline(cin, str);
                    p = str.c_str();
                    continue;
                }
            }
            else {
                s << *p++;
            }
        }
        line.push_back(s.str());
        skipWhite(p); // only applicable if was quoted...
        if (*p == COMMA) {
            p++;
            continue;
        }
        if (*p == 0) {
            break;
        }
        throw std::exception("error: expected comma or end of line (after quote?)");
    }

    return true;
}

vector<string> fields;

void getHeader() {
    getl(fields);
}

/** accepts optinally a top level array of documents [ {}, ... ]
*/
void go() {
    if (header) {
        getHeader();
    }

    vector<string> line;
    while (1) {
        if (cin.eof())
            break;
        line.clear();
        if (!getl(line)) {
            break;
        }
        cerr << "GOT " << line.size() << endl;
        {
            bsonobjbuilder b;
            for ( unsigned i = 0; i < line.size(); i++ ) {
                b.append(fields[i], line[i]);
            }
            bsonobj o = b.obj();
            cout.write(o.objdata(), o.objsize());
        }
    }
}

bool parms(cmdline& c) { 
    if (c.help()) {
        cout << "fromcsv - convert comma separated value (CSV) input to BSON output\n\n";
        cout << "stdin should provide CSV text lines as input.\n";
        cout << "\n";
        cout << "options:\n";
        cout << "  -H      file has a header line\n";
        return true;
    }
    header = c.got("H");
    return false;
}

int main(int argc, char* argv[])
{
    try {
        cmdline c(argc, argv);
        if( parms(c) )
            return 0;
        go();
    }
    catch (std::exception& e) {
        cerr << "fromcsv ";
        cerr << e.what() << endl;
        return EXIT_FAILURE;
    }
    return 0;
}

