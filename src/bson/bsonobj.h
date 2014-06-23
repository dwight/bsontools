/*    Copyright 2009 10gen Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#pragma once

#include <set>
#include <list>
#include <string>
#include <vector>
#include "bsonelement.h"
#include "string_data.h"
#include "builder.h"
#include "ordering.h"

namespace _bson {

//    typedef std::set< BSONElement, BSONElementCmpWithoutField > BSONElementSet;
  //  typedef std::multiset< BSONElement, BSONElementCmpWithoutField > BSONElementMSet;

    /**
       C++ view of a "BSON" object.

       The raw bson buffer referenced is treated as immutable.  There is no transformation -- 
       we aren't creating a DOM or anything -- just scanning the BSON upon request and such.

       See bsonspec.org.

    */
    class bsonobj {
    private:
        const char *_objdata;
        void _assertInvalid() const;
        void init(const char *data) {
            _objdata = data;
            if (!isValid())
                _assertInvalid();
        }

    public:

        /** Construct a bsonobj from data in the proper format.
        */
        explicit bsonobj(const char *msgdata) {
            init(msgdata);
        }

        /** Construct an empty bsonobj -- that is, {}. */
        bsonobj();

        ~bsonobj() {
            _objdata = 0; // defensive
        }

        /** Readable representation of a BSON object in an extended JSON-style notation.
            This is an abbreviated representation which might be used for logging.
        */
        enum { maxToStringRecursionDepth = 100 };

        std::string toString( bool isArray = false, bool full=false ) const;
        void toString( StringBuilder& s, bool isArray = false, bool full=false, int depth=0 ) const;

        /** Properly formatted JSON string.
            @param pretty if true we try to add some lf's and indentation
        */
        std::string jsonString( JsonStringFormat format = Strict, int pretty = 0 ) const;

        /** note: addFields always adds _id even if not specified */
//        int addFields(bsonobj& from, std::set<std::string>& fields); /* returns n added */

        /** remove specified field and return a new object with the remaining fields.
            slowish as builds a full new object
         */
  //      bsonobj removeField(const StringData& name) const;

        /** returns # of top level fields in the object
           note: iterates to count the fields
        */
        int nFields() const;

        /** adds the field names to the fields set.  does NOT clear it (appends). */
        int getFieldNames(std::set<std::string>& fields) const;

        /** @return the specified element.  element.eoo() will be true if not found.
            @param name field to find. supports dot (".") notation to reach into embedded objects.
             for example "x.y" means "in the nested object in field x, retrieve field y"
        */
        BSONElement getFieldDotted(const StringData &name) const;

        /** Like getFieldDotted(), but expands arrays and returns all matching objects.
         *  Turning off expandLastArray allows you to retrieve nested array objects instead of
         *  their contents.
         */
        //void getFieldsDotted(const StringData& name, BSONElementSet &ret, bool expandLastArray = true ) const;
        //void getFieldsDotted(const StringData& name, BSONElementMSet &ret, bool expandLastArray = true ) const;

        /** Like getFieldDotted(), but returns first array encountered while traversing the
            dotted fields of name.  The name variable is updated to represent field
            names with respect to the returned element. */
        BSONElement getFieldDottedOrArray(const char *&name) const;

        /** Get the field of the specified name. eoo() is true on the returned
            element if not found.
        */
        BSONElement getField(const StringData& name) const;

        /** Get several fields at once. This is faster than separate getField() calls as the size of
            elements iterated can then be calculated only once each.
            @param n number of fieldNames, and number of elements in the fields array
            @param fields if a field is found its element is stored in its corresponding position in this array.
                   if not found the array element is unchanged.
         */
        void getFields(unsigned n, const char **fieldNames, BSONElement *fields) const;

        /** Get the field of the specified name. eoo() is true on the returned
            element if not found.
        */
        BSONElement operator[] (const StringData& field) const {
            return getField(field);
        }

        BSONElement operator[] (int field) const {
            StringBuilder ss;
            ss << field;
            std::string s = ss.str();
            return getField(s.c_str());
        }

        /** @return true if field exists */
        bool hasField( const StringData& name ) const { return !getField(name).eoo(); }
        /** @return true if field exists */
        bool hasElement(const StringData& name) const { return hasField(name); }

        /** @return "" if DNE or wrong type */
        const char * getStringField(const StringData& name) const;

        /** @return subobject of the given name */
        bsonobj getObjectField(const StringData& name) const;

        /** @return INT_MIN if not present - does some type conversions */
        int getIntField(const StringData& name) const;

        /** @return false if not present
            @see BSONElement::trueValue()
         */
        bool getBoolField(const StringData& name) const;

        /** @param pattern a BSON obj indicating a set of (un-dotted) field
         *  names.  Element values are ignored.
         *  @return a BSON obj constructed by taking the elements of this obj
         *  that correspond to the fields in pattern. Field names of the
         *  returned object are replaced with the empty string. If field in
         *  pattern is missing, it is omitted from the returned object.
         *
         *  Example: if this = {a : 4 , b : 5 , c : 6})
         *    this.extractFieldsUnDotted({a : 1 , c : 1}) -> {"" : 4 , "" : 6 }
         *    this.extractFieldsUnDotted({b : "blah"}) -> {"" : 5}
         *
        */
        bsonobj extractFieldsUnDotted(const bsonobj& pattern) const;

        /** extract items from object which match a pattern object.
            e.g., if pattern is { x : 1, y : 1 }, builds an object with
            x and y elements of this object, if they are present.
           returns elements with original field names
        */
        bsonobj extractFields(const bsonobj &pattern , bool fillWithNull=false) const;

        bsonobj filterFieldsUndotted(const bsonobj &filter, bool inFilter) const;

        BSONElement getFieldUsingIndexNames(const StringData& fieldName,
                                            const bsonobj &indexKey) const;

        /** @return the raw data of the object */
        const char *objdata() const {
            return _objdata;
        }
        /** @return total size of the BSON object in bytes */
        int objsize() const { return *(reinterpret_cast<const int*>(objdata())); }

        /** performs a cursory check on the object's size only. */
        bool isValid() const;
#if 0
        /** Same as above with the following extra restrictions
         *  Not valid if:
         *      - "_id" field is a
         *          -- Regex
         *          -- Array
         */
        inline bool okForStorageAsRoot() const {
            return _okForStorage(true, true).isOK();
        }

        /**
         * Validates that this can be stored as an embedded document
         * See details above in okForStorage
         *
         * If 'deep' is true then validation is done to children
         *
         * If not valid a user readable status message is returned.
         */
        inline Status storageValidEmbedded(const bool deep = true) const {
            return _okForStorage(false, deep);
        }

        /**
         * Validates that this can be stored as a document (in a collection)
         * See details above in okForStorageAsRoot
         *
         * If 'deep' is true then validation is done to children
         *
         * If not valid a user readable status message is returned.
         */
        inline Status storageValid(const bool deep = true) const {
            return _okForStorage(true, deep);
        }
#endif
        /** @return true if object is empty -- i.e.,  {} */
        bool isEmpty() const { return objsize() <= 5; }

        void dump() const;

        /** Alternative output format */
        std::string hexDump() const;

        /**wo='well ordered'.  fields must be in same order in each object.
           Ordering is with respect to the signs of the elements
           and allows ascending / descending key mixing.
           @return  <0 if l<r. 0 if l==r. >0 if l>r
        */
        int woCompare(const bsonobj& r, const Ordering &o,
                      bool considerFieldName=true) const;

        /**wo='well ordered'.  fields must be in same order in each object.
           Ordering is with respect to the signs of the elements
           and allows ascending / descending key mixing.
           @return  <0 if l<r. 0 if l==r. >0 if l>r
        */
        int woCompare(const bsonobj& r, const bsonobj &ordering = bsonobj(),
                      bool considerFieldName=true) const;

        bool operator<( const bsonobj& other ) const { return woCompare( other ) < 0; }
        bool operator<=( const bsonobj& other ) const { return woCompare( other ) <= 0; }
        bool operator>( const bsonobj& other ) const { return woCompare( other ) > 0; }
        bool operator>=( const bsonobj& other ) const { return woCompare( other ) >= 0; }

        /**
         * @param useDotted whether to treat sort key fields as possibly dotted and expand into them
         */
        int woSortOrder( const bsonobj& r , const bsonobj& sortKey , bool useDotted=false ) const;

        bool equal(const bsonobj& r) const;

        /**
         * @param otherObj
         * @return true if 'this' is a prefix of otherObj- in other words if
         * otherObj contains the same field names and field vals in the same
         * order as 'this', plus optionally some additional elements.
         */
        bool isPrefixOf( const bsonobj& otherObj ) const;

        /**
         * @param otherObj
         * @return returns true if the list of field names in 'this' is a prefix
         * of the list of field names in otherObj.  Similar to 'isPrefixOf',
         * but ignores the field values and only looks at field names.
         */
        bool isFieldNamePrefixOf( const bsonobj& otherObj ) const;

        /** This is "shallow equality" -- ints and doubles won't match.  for a
           deep equality test use woCompare (which is slower).
        */
        bool binaryEqual(const bsonobj& r) const {
            int os = objsize();
            if ( os == r.objsize() ) {
                return (os == 0 || memcmp(objdata(),r.objdata(),os)==0);
            }
            return false;
        }

        /** @return first field of the object */
        BSONElement firstElement() const { return BSONElement(objdata() + 4); }

        /** faster than firstElement().fieldName() - for the first element we can easily find the fieldname without
            computing the element size.
        */
        const char * firstElementFieldName() const {
            const char *p = objdata() + 4;
            return *p == EOO ? "" : p+1;
        }

        BSONType firstElementType() const {
            const char *p = objdata() + 4;
            return (BSONType) *p;
        }

        /** Get the _id field from the object.  For good performance drivers should
            assure that _id is the first element of the object; however, correct operation
            is assured regardless.
            @return true if found
        */
        bool getObjectID(BSONElement& e) const;

        /** @return A hash code for the object */
        int hash() const {
            unsigned x = 0;
            const char *p = objdata();
            for ( int i = 0; i < objsize(); i++ )
                x = x * 131 + p[i];
            return (x & 0x7fffffff) | 0x8000000; // must be > 0
        }

        // Return a version of this object where top level elements of types
        // that are not part of the bson wire protocol are replaced with
        // string identifier equivalents.
        // TODO Support conversion of element types other than min and max.
        bsonobj clientReadable() const;

        /** Return new object with the field names replaced by those in the
            passed object. */
        bsonobj replaceFieldNames( const bsonobj &obj ) const;

        /** true unless corrupt */
        bool valid() const;

        /** @return an md5 value for this object. */
        std::string md5() const;

        bool operator==( const bsonobj& other ) const { return equal( other ); }
        bool operator!=(const bsonobj& other) const { return !operator==( other); }

        enum MatchType {
            Equality = 0,
            LT = 0x1,
            LTE = 0x3,
            GTE = 0x6,
            GT = 0x4,
            opIN = 0x8, // { x : { $in : [1,2,3] } }
            NE = 0x9,
            opSIZE = 0x0A,
            opALL = 0x0B,
            NIN = 0x0C,
            opEXISTS = 0x0D,
            opMOD = 0x0E,
            opTYPE = 0x0F,
            opREGEX = 0x10,
            opOPTIONS = 0x11,
            opELEM_MATCH = 0x12,
            opNEAR = 0x13,
            opWITHIN = 0x14,
            opMAX_DISTANCE = 0x15,
            opGEO_INTERSECTS = 0x16,
        };

        /** add all elements of the object to the specified vector */
        void elems(std::vector<BSONElement> &) const;
        /** add all elements of the object to the specified list */
        void elems(std::list<BSONElement> &) const;

        /** add all values of the object to the specified vector.  If type mismatches, exception.
            this is most useful when the bsonobj is an array, but can be used with non-arrays too in theory.

            example:
              bo sub = y["subobj"].Obj();
              std::vector<int> myints;
              sub.Vals(myints);
        */
        template <class T>
        void Vals(std::vector<T> &) const;
        /** add all values of the object to the specified list.  If type mismatches, exception. */
        template <class T>
        void Vals(std::list<T> &) const;

        /** add all values of the object to the specified vector.  If type mismatches, skip. */
        template <class T>
        void vals(std::vector<T> &) const;
        /** add all values of the object to the specified list.  If type mismatches, skip. */
        template <class T>
        void vals(std::list<T> &) const;

        friend class BSONObjIterator;
        typedef BSONObjIterator iterator;

        /** use something like this:
            for( bsonobj::iterator i = myObj.begin(); i.more(); ) {
                BSONElement e = i.next();
                ...
            }
        */
        BSONObjIterator begin() const;

        void appendSelfToBufBuilder(BufBuilder& b) const {
            verify( objsize() != 0 );
            b.appendBuf(reinterpret_cast<const void *>( objdata() ), objsize());
        }

        template<typename T> bool coerceVector( std::vector<T>* out ) const;

    };

    std::ostream& operator<<( std::ostream &s, const bsonobj &o );
    std::ostream& operator<<( std::ostream &s, const BSONElement &e );

    StringBuilder& operator<<( StringBuilder &s, const bsonobj &o );
    StringBuilder& operator<<( StringBuilder &s, const BSONElement &e );


    struct BSONArray : bsonobj {
        // Don't add anything other than forwarding constructors!!!
        BSONArray(): bsonobj() {}
        explicit BSONArray(const bsonobj& obj): bsonobj(obj) {}
    };

}

#include "bson-inl.h"