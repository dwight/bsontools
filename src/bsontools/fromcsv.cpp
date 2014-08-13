#include <memory>
#include "io.h"
#include <fcntl.h>
#include <iostream>
#include <string>
#include "../../../bson-cxx/src/bson/json.h"
#include "../../../bson-cxx/src/bson/bsonobjbuilder.h"
#include "cmdline.h"

using namespace std;
using namespace _bson;

bool header = false;

const char QUOTE = '"';
char COMMA = ',';

unsigned long long lineNumber = 0;

void skipWhite(const char *& p) {
    while (*p == ' ' || *p == '\t')
        p++;
}

bool getl(vector<string> &line, istream& cin) {
    line.clear();
    string str;
    getline(cin, str);
    if (str.empty())
        return !cin.eof();

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

char DEFAULT_TYPE = 't';
vector<string> fields;
string types;

char getTypeForField(unsigned i) {
    return i < types.size() ? types[i] : DEFAULT_TYPE;
}

void getHeader() {
    lineNumber++;
    getl(fields, cin);
}

void appendAsNumber(bsonobjbuilder& b, const string &f, const char *p) {
    while (*p == ' ') p++;
    bool flt = false;
    {
        const char *q = p;
        if (*q == '-')
            q++;
        for (; *q; q++)
            if (!isdigit(*q))
                flt = true;
    }
    if (flt) {
        double result = 0;
        parseNumberFromString(p, &result);
        b.appendNumber(f, result);
    }
    else {
        long long n = 0;
        parseNumberFromString(p, &n);
        int x = (int)n;
        if (x == n) {
            b.appendNumber(f, x);
        }
        else {
            b.appendNumber(f, n);
        }
    }
}

/** accepts optinally a top level array of documents [ {}, ... ]
*/
void go() {
    _setmode(_fileno(stdout), _O_BINARY);

    if (header) {
        getHeader();
    }
    if (fields.empty())
        throw std::exception("no fieldnames specified or found");

    vector<string> line;
    while (1) {
        if (cin.eof())
            break;
        line.clear();
        lineNumber++;
        if (!getl(line, cin)) {
            break;
        }
        {
            bsonobjbuilder b;
            for ( unsigned i = 0; i < line.size(); i++ ) {
                string f = fields[i];
                string v = line[i];
                char t = getTypeForField(i);
                switch (t) {
                case 'd':
                {
                    StatusWith<Date_t> dateRet = dateFromISOString(v);
                    if (!dateRet.isOK()) {
                        throw std::exception("couldn't parse data string");
                    }
                    b.appendDate(f, dateRet.getValue());
                    break;
                }
                case 'f':
                {
                    double d = 0;
                    parseNumberFromString(v, &d);
                    b.appendNumber(f, d);
                    break;
                }
                case 'l':
                {
                    long long d = 0;
                    parseNumberFromString(v, &d);
                    b.appendNumber(f, d);
                    break;
                }
                case 'i':
                {
                    int d = 0;
                    parseNumberFromString(v, &d);
                    b.appendNumber(f, d);
                    break;
                }
                case 'n':
                    appendAsNumber(b, f, v.c_str());
                    break;
                case 't':
                    b.append(f, v);
                    break;
                default:
                    throw std::exception("unexpected type (-t) encountered");
                }
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
        cout << "  -H              file has a header line\n";
        cout << "  -f <fields>     comma separated list of field names\n";
        cout << "  -t <types>      optional list of types for each field. list can be shorter than all fields\n";
        cout << "                  in which case default type is used for remainder.\n";
        cout << "  -T              parse as tab delimited rather than comma delimited\n";
        cout << '\n';
        cout << "types allowed in the -t parameter:\n";
        cout << "  t               text/string.  default.\n";
        cout << "  n               number (auto detect numeric type)\n";
        cout << "  i               int32\n";
        cout << "  l               int64\n";
        cout << "  f               float (double)\n";
        cout << "  d               date - parsed as an ISO date string\n";
        cout << "\nExample:\n\n";
        cout << "  fromcsv -f name,age,city,state -t tntt < file.csv > file.bson\n";
        return true;
    }
    if (c.got("T")) {
        COMMA = '\t';
    }
    header = c.got("H");
    string s = c.getStr("f");
    if (!s.empty()) {
        if (header)
            throw std::exception("can't specify both -H and -f");
        cerr << "-f " << s << endl;
        istringstream ss(s);
        getl(fields, ss);
    }
    {
        string t = c.getStr("t");
        types = t;
    }
    return false;
}

int main(int argc, char* argv[])
{
    try {
        set<string> parmOptions = { "f" };
        cmdline c(argc, argv, &parmOptions);
        if( parms(c) )
            return 0;
        go();
    }
    catch (std::exception& e) {
        cerr << "fromcsv ";
        cerr << e.what();
        if (lineNumber) {
            cerr << " line:" << lineNumber;
        }
        cerr << endl;
        return EXIT_FAILURE;
    }
    return 0;
}

