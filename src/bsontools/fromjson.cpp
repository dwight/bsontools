// fromjson.cpp : Defines the entry point for the console application.
//

#include <memory>
#include <iostream>
#include <string>
#include "../../../bson-cxx/src/bson/json.h"
#include "../../../bson-cxx/src/bson/bsonobjbuilder.h"
#include "cmdline.h"

using namespace std;
using namespace _bson;

void go() {
    while (1) {
        while (1) {
            if (cin.eof())
                goto done;
            char ch = cin.peek();
            if (ch == -1 || ch == 26 /*EOF*/ || cin.eof() )
                goto done;
            if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
                cin.get();
                continue;
            }
            break;
        }
        bsonobjbuilder b;
        bsonobj o = fromjson(cin, b);
        cout.write(o.objdata(), o.objsize());
    }
    done:
    return;
}

bool parms(cmdline& c) { 
    if (c.help()) {
        cout << "fromjson utility - convert JSON input to BSON output\n\n";
        cout << "options:\n";
        cout << "  coming soon...\n\n";
        cout << "stdin should provide JSON documents as input. Documents may span lines in the\n";
        cout << "input. Quotes around fieldnames on the left hand side are generally optional.\n";
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
        cerr << "fromjson ";
        cerr << e.what() << endl;
        return EXIT_FAILURE;
    }
    return 0;
}

