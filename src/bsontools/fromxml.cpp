// fromjson.cpp : Defines the entry point for the console application.
//

#include <memory>
#include <iostream>
#include <string>
#include "binary.h"
#include "../../../bson-cxx/src/bson/json.h"
#include "../../../bson-cxx/src/bson/bsonobjbuilder.h"
#include "cmdline.h"

using namespace std;
using namespace _bson;

unsigned line = 1;

class parse_error : public std::exception {
    const string msg;
public:
    parse_error(string s) : msg(string("parse error: " + s)) {}
    const char *what() const throw() { return msg.c_str(); }
};
char BeginTag = '<';
const char * EndTag = "/>";

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
    char getc_ws() {
        unsigned char ch;
        bool ws;
        do {
            ch = get();
            ws = is_ws(ch);
        } while (ws);
        return ch;
    }
    // <NAME ... />
    string get_name() {
        StringBuilder s;
        s << getc_ws();
        while (1) {
            char ch = i.peek();
            if (is_ws(ch))
                break;
            if (ch == '/' || ch == '>')
                break;
            s << ch; 
            i.get();
        }
        return s.str();
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
        if (!has_attributes()) {
            if (has(EndTag)) {
                // <foo/> -- we could ignore or emit foo:null for the field
                b.appendNull(nm);
            }
            else {
                eat(">");
                if (is_begin_peek()) {
                    bsonobjbuilder nest;
                    string lastFieldName;
                    while (1) {
                        // <foo><bar>99</bar><bar>33</bar></foo>
                        //      ^
                        bsonobjbuilder temp;
                        go(temp);
                        if (has("</"))
                            break; // </foo>
                        // arrays? 
                    }
                    b.append(nm, nest.obj());
                    eat(nm.c_str());
                    eat(">");
                }
                else {
                    // <foo>99</foo>
                    //      ^
                    string s = get_char_data();
                    b.append(nm, s);
                    eat("</");
                    eat(nm.c_str());
                    eat(">");
                }
            }
        }
        else {
                // <foo attrib1=asdf ... >
                throw parse_error("finish code");
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
        cout << "  -ia       ignore attributes in tags\n";
        cout << "  -i"
        return true;
    }
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
        cerr << "fromxml: ";
        cerr << e.what() << " line: " << line << endl;
        return EXIT_FAILURE;
    }
    return 0;
}

