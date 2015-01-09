hex - BSON-friendly hex dump utility.

## Usage

	hex <options> < infile.bson

Options:

* -h help
* -v verbose style dump
* -raw raw hex dump with no 'bson awareness'
 
## Purpose

This is a very simple hex dump utility which could be used 
with any binary file.  However, adds a tiny amount of BSON 
awareness, which can be useful when dumping larger BSON files.

Specifically, it is aware that the first four bytes of a BSON 
document indicate its size.  It will output the dump of those bytes
on a line by itself, then, the rest of that document, then, a blank
separator line.  Thus it makes it easy to find the point of demarcation
for each BSON document in the dump.  Trivial but very useful.

## Example

	~/bsontools $ ./fromjson > out.bson
	{"hello": "world"}
	{"BSON": ["awesome", 5.05, 1986]}

	~/bsontools $ ./hex < out.bson
	16 00 00 00 
	02 68 65 6C 6C 6F 00 06 00 00 00 77 6F 72 6C 64 
	00 00 
	
	31 00 00 00 
	04 42 53 4F 4E 00 26 00 00 00 02 30 00 08 00 00 
	00 61 77 65 73 6F 6D 65 00 01 31 00 33 33 33 33 
	33 33 14 40 10 32 00 C2 07 00 00 00 00 

	~/bsontools $ ./hex -v < out.bson 
	16 00 00 00                                         22
	02 68 65 6C 6C 6F 00 06 00 00 00 77 6F 72 6C 64     .hello.....world
	00 00                                               ..
	
	31 00 00 00                                         49
	04 42 53 4F 4E 00 26 00 00 00 02 30 00 08 00 00     .BSON.&....0....
	00 61 77 65 73 6F 6D 65 00 01 31 00 33 33 33 33     .awesome..1.3333
	33 33 14 40 10 32 00 C2 07 00 00 00 00              33.@.2.......

	~/bsontools $ ./hex -raw < out.bson
	16 00 00 00 02 68 65 6C 6C 6F 00 06 00 00 00 77 
	6F 72 6C 64 00 00 31 00 00 00 04 42 53 4F 4E 00 
	26 00 00 00 02 30 00 08 00 00 00 61 77 65 73 6F 
	6D 65 00 01 31 00 33 33 33 33 33 33 14 40 10 32 
	00 C2 07 00 00 00 00 

	~/bsontools $ 
