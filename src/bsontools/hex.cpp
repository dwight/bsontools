#include <memory>
#include <iostream>
#include "stdio.h"
#include "fcntl.h"
#include "io.h"
#include "../../../bson-cxx/src/bson/hex.h"

using namespace std;

int main(int argc, char* argv[])
{
    if (argc > 1) {
        cout << "hex dump utility\n\n";
        cout << "input from stdin will output in hexadecimal" << endl;
        return 0;
    }

    int result = _setmode(_fileno(stdin), _O_BINARY);

    char ch;
    long i = 0;
    while ( 1 ) {
        if (cin.eof())
            break;
        cin.read(&ch, 1);
        string s = _bson::toHex(&ch, 1);
        cout << s << ' ';
        if (++i % 8 == 0)
            cout << endl;
    }

	return 0;
}

