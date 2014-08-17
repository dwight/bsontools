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

/** accepts optinally a top level array of documents [ {}, ... ]
*/
void go() {
    binaryStdOut();

    bool started = false;
    bool isArray = false;
    bool readyForComma = false;
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
            if (ch == ',' && readyForComma) {
                readyForComma = false;
                cin.get();
                continue;
            }
            if (ch == '[') {
                if (!started) {
                    isArray = true;
                    cin.get();
                    continue;
                }
            }
            if (ch == ']' && isArray ) {
                //cerr << "DEBUG GOT ]" << endl;
                cin.get();
                isArray = readyForComma = false;
                continue;
            }
            break;
        }
        started = true;
        bsonobjbuilder b;
        bsonobj o = fromjson(cin, b);
        cout.write(o.objdata(), o.objsize());
        readyForComma = isArray;
    }
done:
    if (isArray) {
        throw bsontool_error("expected ']' at end of file?");
    }
    return;
}

bool parms(cmdline& c) { 
    if (c.help()) {
        cout << "fromjson utility - convert JSON input to BSON output\n\n";
        cout << "stdin should provide JSON documents as input. Documents may span lines in the\n";
        cout << "input. Quotes around fieldnames on the left hand side are generally optional.\n\n";
        cout << "Optionally, the entire input stream can be an array, in which case each array\n";
        cout << "element must be a document; in this case each document in the array will be emitted\n";
        cout << "as BSON.\n\n";
        cout << "Example:\n\n";
        cout << "\t$ fromjson > out.bson\n";
        cout << "\t{x:3} {y:2}\n";
#if defined(_WIN32)
        cout << "\t^Z\n";
#else
        cout << "\t^d\n";
#endif
        cout << "\t$ bson print < out.bson\n";
        cout << "\t{ x: 3}\n";
        cout << "\t{ y: 2}\n\n";
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

