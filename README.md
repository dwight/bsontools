These are simple utilities for manipulation of bson documents.

bcount   - count # of bson documents in a bson file
fromjson - convert JSON to BSON
hex      - hex dump of any input.  provided as a convenience for 
           any OS where 'hexdump' is not already present.

Note: many of these utilities expect input from stdin, to facilitate piping.  Use -h to 
      get help.

## Building

You will need the bson-cxx library (a dependancy) to build the tools.  It is available in github. 
Place it as a peer level directory on disk with bsontools.  (That is you will see 
include "../../../bson-cxx/" in the source code of these tools...)

