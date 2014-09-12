fromxml utility -- converts XML to BSON.

NOTE: PRELIMINARY IMPLEMENTATION (at time of this writing).

## Usage

  fromxml <options> < infile.xml > outfile.bson

Run fromxml -h for options.

### Interpretation of structure

(Note this may evolve, but this version...)

<foo>asdf</foo>

becomes:

{ foo: "asdf" }

<foo x=33.3>asdf</foo> becomes:

{ foo: { x: "33.3", foo: "asdf" } }

<a><b>3</b><c>asdf</c></a> becomes:

{ a: { b: "3", c: "asdf" } }

### Numerics

Use -t <options> or -f <field>=options to set rules for interpretation of datatypes.

<x>3</x>

would by default yield: { x : "3" }

with "-t n": { x : 3 } (where the 3 is an int32)

with "-t f": { x : 3.0 }

That is to say the 'n' option uses the smallest type that makes sense.

More data types will be done later hopefully.
