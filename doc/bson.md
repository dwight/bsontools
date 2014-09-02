The "bson" utility program.

## Usage

  bson <options> <command> <parms>

Options:
  -h                          for help
  -#                          emit document number instead of the document's bson content
  -N                          emit null rather than nothing, when applicable (eg w/pull)

The bson utility performs many different operations; these are specified by the <command> 
parameter.  Typically, the utility takes a stream of BSON from stdin, and writes BSON to 
stdout.  However, in some cases, it outputs text, for example, the print verb/command does
that.

## Commands

### count

Count number of documents in the BSON file.  Also prints out the size of the largest document.

    $ bson count < file.bson
    302    12800

### del <fieldname>

Delete a top level field and then reoutput the stream.

    $ bson del card_number < records.bson > redacted.bson

### demote <fieldname>

Wrap each document inside a field named <fieldname>, then output.

For example, suppose we have a set of JSON documents of the form:

    { street:..., city:..., province:..., country:... }
    ...

If we wanted this to be in a larger document we might use the demote command to "wrap" these 
values:

    bson demote addr < in.bson > out.bson

If we then inspected:

    bson print < out.bson

we would see something like:

    { addr : { street:..., city:..., province:..., country:... } }
    ...

And this output could be used then with say, a merge operation.

### grep <pattern>

Search for a pattern within each documents.  Field values, and not the field names, are checked
for a match.  All matching documents are output (as BSON).

    $ bson grep foo < file.bson > out.bson

Note: at the time of this writing, pattern can only be a simple text pattern, regular 
expressions are NOT yet supported (but hopefully coming).

### head [-n <N>] [-s <S>]

Output the first N documents in the file.  Output is BSON format.

-s option: start at the specified document number (base zero).

### infer

The infer command will take a stream of BSON from stdin and infer its "schema"; that is, it 
will report what fields are present in documents read from the stream, and the types of those
fields.  If a field is not always present, infer will report the percentage of the time it 
occurs.  If a field's values have different types in different documents, infer will report 
this too.  The output from infer is a text report.

Example:

    $ bson infer < x.bson
    {
      a : int32
        b : bool 50%
        c : bool 50%, null 50%
        d : string 50%
        e : object {
        valid : bool
        until : date
      }
    }

### merge <filename>

Merge bson stream stdin with bson file on cmd line.  Must be same number of documents in 
each input file.

### print

    $ bson print < mydata.bson
    *json output is displayed...*

### project <fieldnamelist>

Do projection on the input BSON, outputing the result.  Only top level field names may be 
specified in the current implementation.

    $ bson project "last name","first_name",phone < customers.bson > phonelist.bson

### promote <fieldname>

Pull out the subobject specified by <fieldname> and output it (only).  If the field is missing
or not an object nothing will be output for that document.

    $ bson promote associate.address < things.bson > addresses.bson

### pull [-N] <fieldname>

Extract a single field as a singleton element and output.

-N option: emit null rather than nothing, when applicable.

Example:
        
    $ bson print < t.bson
    { a: 3, b: true, c: { x: 3, y: 4 } }
    { a: 2, c: { x: 2, y: 9 } }
    { c: null }
    { a: 1 }
    
    $ bson pull a < t.bson | bson print
    { a: 3 }
    { a: 2 }
    {}
    { a: 1 }
    
    bson pull c < t.bson | bson print
    { c: { x: 3, y: 4 } }
    { c: { x: 2, y: 9 } }
    { c: null }
    {}
    
    pull c.y < t.bson | bson print
    { y: 4 }
    { y: 9 }
    {}
    {}

### sample -n <N>

sample outputs every Nth document in the file for sampling purposes.  Output is bson format.

    $ bson sample -n 100 < bigfile.bson > sample.bson

### tail [-n <N>]

Output the last N documents.  

Note: the implementation of tail is currently not very fast.

### text

Extract and output the text of each BSON document (perhaps for use then with text utilities etc.)

The text from each document is output one line per document, with the text of each field tab 
delimited.  Field names are not output.  Numbers are output in their text representation.

    $ # how many docs contain the text "Joseph"?
    $ bson text < docs.bson | grep Joseph | wc

### unwind [-N] <fieldname>

Unwind an array of elements.  This is similar to the MongoDB $unwind operator.

-N option: emit null rather than nothing, when applicable.

Example:

    $ fromjson > a.bson
    { a : 3, b : [1,2,7,8,"zzz"] }
    { b : [4,7] }
    ^Z
    
    $ bson print < a.bson
    { a: 3, b: [ 1, 2, 7, 8, "zzz" ] }
    { b: [ 4, 7 ] }
    
    $ bson unwind b < a.bson | bson print
    { b: 1 }
    { b: 2 }
    { b: 7 }
    { b: 8 }
    { b: "zzz" }
    { b: 4 }
    { b: 7 }

## Other Options

-# option: tells the utility to output document numbers rather than the actual document as 
output.  Simply used for troubleshooting; say, if you are getting an error mid-file and want 
to track it down.

## Other Notes

You can use the regular Unix 'cat' utility to concatenate BSON files, as it simply concatenates
streams of bytes.

