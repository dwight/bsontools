// fromjson.cpp : Defines the entry point for the console application.
//

#include <memory>
#include <iostream>
#include "../bson/json.h"
#include "../bson/bsonobjbuilder.h"

using namespace std;
using namespace _bson;

int main(int argc, char* argv[])
{

    while (1) {
        if (cin.eof())
            break;
        BSONObjBuilder b;
        bsonobj o = fromjson(cin, b);
        cout.write(o.objdata(), o.objsize());
    }
    
    return 0;
}

