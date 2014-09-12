// fromjson.cpp : Defines the entry point for the console application.
//

#include <memory>
#include <iostream>
#include <string>
#include "binary.h"
#include "../../../bson-cxx/src/bson/json.h"
#include "../../../bson-cxx/src/bson/bsonobjbuilder.h"
#include "cmdline.h"
#include "parse_types.h"

using namespace std;
using namespace _bson;

struct Opts  {
    bool autoNum = false;
} opts;
bool ignoreAttributes = false;
unsigned line = 1;

class parse_error : public std::exception {
    const string msg;
public:
    parse_error(string s) : msg(string("parse error: " + s)) {}
    const char *what() const throw() { return msg.c_str(); }
};
char BeginTag = '<';

class parser {
    bool whiteSpace[256];
    bool is_ws(char ch) {
        return whiteSpace[(unsigned char)ch];
    }
public:
    parser(istream& is) : i(is) {
        for (int i = 0; i < 256; i++)
            whiteSpace[i] = false;
        whiteSpace[' '] = true;
        whiteSpace['\t'] = true;
        whiteSpace['\r'] = true;
        whiteSpace['\n'] = true;
    }
private:
    istream& i;

    void append(bsonobjbuilder& b, string f, string val) {
        if (opts.autoNum) {
            if (mightBeNumber(val.c_str())) {
                if (appendAsNumber(b, f, val.c_str())) {
                    return;
                }
            }
        }
        b.append(f, val);
    }

    char peek() { return i.peek(); }
    bool has_peek(char c) {
        return i.peek() == c;
    }
    void skip_ws() {
        while (is_ws(i.peek())) {
            i.get();
        }
    }
    bool has_attributes() {
        skip_ws();
        char ch = i.peek();
        return !(ch == '/' || ch == '>');
    }
    char get() {
        char ch = i.get();
        if (i.eof()) {
            throw parse_error("unexpected EOF");
        }
        if (ch == '\n')
            line++;
        return ch;
    }
    char getc_ws() {
        unsigned char ch;
        bool ws;
        do {
            ch = get();
            ws = is_ws(ch);
        } while (ws);
        return ch;
    }
    // get identifier, e.g. tag name, or attribute naem
    // <NAME ... />
    string get_name() {
        StringBuilder s;
        s << getc_ws();
        while (1) {
            char ch = i.peek();
            if (is_ws(ch))
                break;
            if (ch == '/' || ch == '>' || ch == '=' )
                break;
            s << ch;
            i.get();
        }
        return s.str();
    }
    // get a string that may be quoted. e.g. value of an attribute
    // <name f="VALUE" />
    string get_value() {
        skip_ws();
        if (peek() == '"') {
            StringBuilder s;
            eat('"');
            while (1) {
                char ch = get();
                // todo: escaping
                if (ch == '"')
                    break;
                s << ch;
            }
            return s.str();
        }
        return get_name();
    }
    void eat(char s) {
        char ch = get();
        if (s != ch) {
            throw parse_error(string("parse error got: ") + ch + " expected: " + s);
        }
    }
    void eat(const char *s) {
        while (*s) {
            char ch = get();
            if (*s != ch) {
                throw parse_error(string("parse error got: ") + ch + " expected: " + s);
            }
            s++;
        }
    }
    bool has(const char *s) {
        const char *p = s;
        while (1) {
            if (*p == 0)
                return true;
            if (i.peek() != *p)
                break;
            i.get();
            p++;
        }
        while (s < p) {
            i.putback(*s++);
        }
        return false;
    }
    bool is_begin_peek() {
    again:
        skip_ws();
        if (i.peek() != BeginTag)
            return false;
        i.get();
        if (i.peek() == '!') {
            // <!--  -->
            // <!DOCTYPE >
            while (get() != '>') {
                ;
            }
            goto again;
        }
        i.putback(BeginTag);
        return true;
    }
    // get value between tags
    // todo: escaping
    string get_char_data() {
        StringBuilder b;
        while (1) {
            char ch = i.peek();
            if (ch == BeginTag)
                break;
            b << ch;
            get();
        }
        return b.str();
    }
public:
    bool go(bsonobjbuilder& b) {        
        bool begin = false;
        try {
            begin = is_begin_peek();
            if (begin) {
                get(); // consume
            }
        }
        catch (parse_error&) {
            // eof
            return false;
        }
        if (!begin) {
            if (i.eof())
                return false;
            throw parse_error("unexpected char, expected '<'");
        }
        string nm = get_name();
        bsonobjbuilder subbldr;
        bsonobjbuilder *bldr = &b;
        if (has_attributes()) {
            // <foo attrib1=asdf ... >
            bldr = &subbldr;
            while (1) {
                string f = get_name();
                eat('=');
                string v = get_value();
                if ( !ignoreAttributes )
                    append(*bldr,f, v);
                skip_ws();
                if (peek() == '/' || peek() == '>')
                    break;
            }
        }
        if (has("/>")) {
            // <foo/> -- we could ignore or emit foo:null for the field (if no attributes)
            //b.appendNull(nm);
        }
        else {
            eat(">");
            if (is_begin_peek()) {
                string lastFieldName;
                bldr = &subbldr;
                while (1) {
                    // <foo><bar>99</bar><bar>33</bar></foo>
                    //      ^
                    go(subbldr);
                    if (has("</"))
                        break; // </foo>
                    // arrays? 
                }
                eat(nm.c_str());
                eat(">");
            }
            else {
                // <foo>99</foo>
                //      ^
                string s = get_char_data();
                append(*bldr, nm, s);
                eat("</");
                eat(nm.c_str());
                eat(">");
            }
        }
        if (bldr != &b) {
            b.append(nm, subbldr.obj());
        }
        return true;
    }
};


/** accepts optinally a top level array of documents [ {}, ... ]
*/
void go() {
    binaryStdOut();

    parser p(cin);
    while (1) {
        bsonobjbuilder b;
        if (!p.go(b))
            break;

        cout << "debug - Parsed to: " << endl;
        cout << b.obj().toString() << endl;
        cout << endl;
    }
    cout << endl;
}

bool parms(cmdline& c) { 
    if (c.help()) {
        cout << "fromxml utility - convert XML input to BSON output\n\n";
        cout << "stdin should provide XML documents as input.\n\n";
        cout << "options:\n";
        cout << "  -ia             ignore attributes in tags\n";
        cout << "  -t options      how to handle types, see below\n";
        cout << "\n";
        cout << "options for the t parameter:\n";
        cout << "  n               auto detect numbers\n";
        /*
        cout << "  s               auto detect small numbers only\n";
        cout << "  i               make #s int32 when possible\n";
        cout << "  l               make #s int64 when possible\n";
        cout << "  f               float (double)\n";
        cout << "  d               auto detect date types\n";
        */
        cout << "\n";
        return true;
    }
    ignoreAttributes = c.got("ia");
    {
        string t = c.getStr("t");
        for (auto i = t.begin(); i != t.end(); i++) {
            if (*i == 'n') {
                opts.autoNum = true;
            } 
            else {
                cerr << "unknown command line option in -t parameter" << endl;
                return true;
            }
        }
    }
    return false;
}

int main(int argc, char* argv[])
{
    try {
        set<string> optionsWithParameter = {"t"};
        cmdline c(argc, argv, &optionsWithParameter);
        if( parms(c) )
            return 0;
        go();
    }
    catch (std::exception& e) {
        cerr << "fromxml: ";
        cerr << e.what() << " line: " << line << endl;
        return EXIT_FAILURE;
    }
    return 0;
}

