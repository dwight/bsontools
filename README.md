These are simple utilities for manipulation of bson documents.

* bson     - main utility.  Try bson --help
* fromjson - convert JSON to BSON
* fromcsv  - convert CSV to BSON
* fromxml  - convert XML to BSON
* hex      - hex dump of any input, with a tiny bit of bson format awareness.  
             also provided as a convenience for any OS where 'hexdump' is not already present.

Note: Many of these utilities expect input from stdin, to facilitate piping. Use -h to get help.

## Documentation

See the [doc/](doc) folder.

## Building

You will need the [bson-cxx](https://github.com/dwight/bson-cxx) library (a dependancy) to build the tools.  It is available on github.

Place it as a peer level directory on disk with bsontools.  (That is you will see lines such as `#include "../../../bson-cxx/"` in the source code of these tools...)

To build with scons, assuming it is installed, just type "scons".

With Visual Studio, start by opening build/fromjson/fronjson.sln. The tools have been built and tested with Visual Studio 2013.

As written, these tools lightly use C++11.  This is mainly to avoid any external dependencies; for 
example unique_ptr is used from C++11. The usage is light enough you should not need a very new compiler. It would not be hard to adapt back to C++03. Use `-std=c++0x` on the command line, which the SConscript file does for you...

### Prebuilt binaries

Some can be found at [https://github.com/dwight/binaries](https://github.com/dwight/binaries).

## Licence

Apache 2.0.

## Contributions

Send a pull request on github, thanks. We'll pull in things that we think fit although the goal is to keep things tidy and small.  Of course you can always fork if you want to create something different.

Some automated tests would be real helpful for example...

## Support

For help try posting to the [BSON Google Groups forum](https://groups.google.com/forum/#!forum/bson).




