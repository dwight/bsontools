#include <memory>
#include <iostream>
#include "stdio.h"
#include "fcntl.h"
#include "io.h"
#include "../../../bson-cxx/src/bson/hex.h"
#include "../../../bson-cxx/src/bson/endian.h"
#include "cmdline.h"

using namespace std;

int main(int argc, char* argv[])
{
    cmdline c(argc, argv);

    if (c.help()) {
        cout << "Hex dump utility. ";
        cout << "Input from stdin will output in hexadecimal.\n" << endl;
        cout << "This utility has a tiny amount of BSON awareness.  It will output (in hex)\n";
        cout << "the total BSON object size, then on subsequent lines the document's content in hex.\n";
        cout << "Documents are separating by a blank line.  If non-bson data is encountered, it will\n";
        cout << "simply be dumped and no error indicated.\n\n";
        cout << "Options:\n";
        cout << "  -h    help\n";
        cout << "  -raw  raw hex dump with no 'bson awareness'" << endl;
        return 0;
    }

    bool raw = c.got("raw");

    int result = _setmode(_fileno(stdin), _O_BINARY);

    // warning: this code is pretty spaghetti as i was just prototyping / iterating 
    //          and this is just a very minor utility so didn't care.
    char ch;
    long i = 0;
    int j = 0;
    char hist[4];
    int state = 4;
    while ( 1 ) {
        cin.read(&ch, 1);
        if (cin.eof())
            break;
        hist[(j++) % 4] = ch;
        state--;
        string s = _bson::toHex(&ch, 1);
        cout << s << ' ';
        if (state == 0) {
            int sz = _bson::readInt(hist);
            state = sz;
            //cout << " (" << sz << ")";
            if (!raw) {
                i = 0;
                cout << endl;
                continue;
            }
        }
        else if (state == 4) {
            j = 0; 
            if (!raw) {
                i = 0;
                cin.peek();
                if (!cin.eof() )
                    cout << "\n\n";
                continue;
            }
        }
        if (++i % 8 == 0) {
            if (  i % 16 == 0 )
                cout << '\n';
        }
    }

	return 0;
}

