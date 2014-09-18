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
#include "../util/str.h"

//#include "windows.h"

using namespace std;
using namespace _bson;

Opts opts;
bool ignoreAttributes = false;
unsigned line = 1;
map<string, Opts*> fieldOptions;

//#define debug(x) (cout << x ) << endl;
#define debug(x) 

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
        init();
    }
private:
    istream& i;

    void append(bsonobjbuilder& b, string f, string val) {
        const Opts *o = &opts;
        auto i = fieldOptions.find(f);
        if (i != fieldOptions.end()) {
            o = i->second;
        }
        if (o->autoNum) {
            if (mightBeNumber(val.c_str())) {
                if (appendAsNumber(b, f, val.c_str(), *o)) {
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
            debug("unexpected EOF ch:" << ch)
            //DebugBreak();
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
    map<string, char> escapes;
    void init() {
        escapes["amp"] = '&';
        escapes["quot"] = '"';
        escapes["apos"] = '\'';
        escapes["lt"] = '<';
        escapes["gt"] = '>';
    }
    string unescape(string s) {
        if (!(str::contains(s, '&') && str::contains(s, ';'))) {
            return s;
        }
        StringBuilder b;
        const char *p = s.c_str();
        while (*p) {
            if (*p == '&') {
                string x(p + 1);
                string l, r;
                str::splitOn(x, ';', l, r);
                map<string,char>::const_iterator i = escapes.find(l);
                if (i != escapes.end()) {
                    b << i->second;
                    p += l.size() + 2;
                    continue;
                }
            }
            b << *p++;
        }
        return b.str();
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
            return unescape(s.str());
        }
        return unescape(get_name());
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
        skip_ws();
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
        // todo <!--  -->
        char ch = i.peek();
        if (ch == '!' || ch == '?' ) {
            // <!DOCTYPE >
            // <?xml ... >
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
        return unescape(b.str());
    }
public:
    // see 
    // http://cs.lmu.edu/~ray/notes/xmlgrammar/
    bool go(bsonobjbuilder& b) {  
        debug("\ngo")
        bool begin = false;
        try {
            begin = is_begin_peek();
            if (begin) {
                get(); // consume '<'
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
        debug("nm: " << nm)
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
            b.appendNull(nm);
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
                    if (has("</")) {
                        debug("has </, break")
                        break; // </foo>
                    }
                    else {
                        debug("no </, got " << peek())
                    }
                    skip_ws();
                    debug("arrays? " << peek())
                    // arrays? 
                }
                debug("eat1 " << nm)
                eat(nm.c_str());
                debug("eat >")
                eat(">");
                debug("done2")
            }
            else {
                // <foo>99</foo>
                //      ^
                string s = get_char_data();
                debug("got " << s)
                append(*bldr, nm, s);
                debug("eat </")
                eat("</");
                debug("eat " << nm)
                eat(nm.c_str());
                debug("eat >")
                eat(">");
                debug("done1")
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

        bsonobj o = b.obj();
        cout.write(o.objdata(), o.objsize());

        /*
        cout << "debug - Parsed to: " << endl;
        cout << b.obj().toString() << endl;
        cout << endl;
        */
    }
}

void parseOptions(Opts& opts, string t) {
    for (auto i = t.begin(); i != t.end(); i++) {
        if (*i == 'n') {
            opts.autoNum = true;
        }
        else if (*i == 'f') {
            opts.floatPt = true; 
            opts.autoNum = true;
        }
        else {
            throw std::exception("unknown command line option in -t parameter");
        }
    }
}

bool parms(cmdline& c) { 
    if (c.help()) {
        cout << "fromxml utility - convert XML input to BSON output\n\n";
        cout << "stdin should provide XML documents as input.\n\n";
        cout << "options:\n";
        cout << "  -ia                     ignore attributes in tags\n";
        cout << "  -t typeoptions          how to handle types, see below\n";
        cout << "  -f field=typeoptions    use typeoptions for field\n";
        cout << "\n";
        cout << "typeoptions for the t and f parameter:\n";
        cout << "  n                       auto detect numbers\n";
        /*
        cout << "  s               auto detect small numbers only\n";
        cout << "  i               make #s int32 when possible\n";
        cout << "  l               make #s int64 when possible\n";
        */
        cout << "  f                       make numbers floating pointing point (even if whole)\n";
        cout << "                          implies n option\n";
        /*
        cout << "  d               auto detect date types\n";
        */
        cout << "Examples:\n";
        cout << "\n";
        cout << "  fromxml -t n < in.xml > out.bson\n";
        cout << "  fromxml -f x=n < in.xml\n";
        cout << "  fromxml -f x=n -f y=n -f z=f < in.xml | bson print\n\n";
        return true;
    }
    ignoreAttributes = c.got("ia");
    {
        string t = c.getStr("t");
        parseOptions(opts, t);
        for (auto i = c.options.begin(); i != c.options.end(); i++) {
            if (i->opt == "f") {
                string field, cmdLineFOptionVal;
                str::splitOn(i->value, '=', field, cmdLineFOptionVal);
                assert(!cmdLineFOptionVal.empty());
                Opts *x = new Opts();
                parseOptions(*x, cmdLineFOptionVal);
                fieldOptions[field] = x;
            }
        }
    }
    return false;
}

int main(int argc, char* argv[])
{
    try {
        set<string> optionsWithParameter = {"t", "f"};
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

