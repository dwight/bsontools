The "bson" utility program.

bson -h for help.

Usage:

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

### print

    $ bson print < mydata.bson
    *json output is displayed...*

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

