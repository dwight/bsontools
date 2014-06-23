#include <string>
#include "bsonobjbuilder.h"
#include "bsonobjiterator.h"

using namespace std;

namespace _bson {

    const string BSONObjBuilder::numStrs[] = {
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
        "10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
        "20", "21", "22", "23", "24", "25", "26", "27", "28", "29",
        "30", "31", "32", "33", "34", "35", "36", "37", "38", "39",
        "40", "41", "42", "43", "44", "45", "46", "47", "48", "49",
        "50", "51", "52", "53", "54", "55", "56", "57", "58", "59",
        "60", "61", "62", "63", "64", "65", "66", "67", "68", "69",
        "70", "71", "72", "73", "74", "75", "76", "77", "78", "79",
        "80", "81", "82", "83", "84", "85", "86", "87", "88", "89",
        "90", "91", "92", "93", "94", "95", "96", "97", "98", "99",
    };

    std::string OID::str() const { return toHexLower(data, kOIDSize); }

    // This is to ensure that BSONObjBuilder doesn't try to use numStrs before the strings have been constructed
    // I've tested just making numStrs a char[][], but the overhead of constructing the strings each time was too high
    // numStrsReady will be 0 until after numStrs is initialized because it is a static variable
    bool BSONObjBuilder::numStrsReady = (numStrs[0].size() > 0);

    // wrap this element up as a singleton object.
    bsonobj BSONElement::wrap() const {
        BSONObjBuilder b(size() + 6);
        b.append(*this);
        return b.obj();
    }

    bsonobj BSONElement::wrap(const StringData& newName) const {
        BSONObjBuilder b(size() + 6 + newName.size());
        b.appendAs(*this, newName);
        return b.obj();
    }
    /* add all the fields from the object specified to this object */
    BSONObjBuilder& BSONObjBuilder::appendElements(bsonobj x) {
        if (!x.isEmpty())
            _b.appendBuf(
            x.objdata() + 4,   // skip over leading length
            x.objsize() - 5);  // ignore leading length and trailing \0
        return *this;
    }

    std::string BSONElement::toString(bool includeFieldName, bool full) const {
        StringBuilder s;
        toString(s, includeFieldName, full);
        return s.str();
    }
    void BSONElement::toString(StringBuilder& s, bool includeFieldName, bool full, int depth) const {

        if (depth > bsonobj::maxToStringRecursionDepth) {
            // check if we want the full/complete string
            if (full) {
                StringBuilder s;
                s << "Reached maximum recursion depth of ";
                s << bsonobj::maxToStringRecursionDepth;
                uassert(16150, s.str(), full != true);
            }
            s << "...";
            return;
        }

        if (includeFieldName && type() != EOO)
            s << fieldName() << ": ";
        switch (type()) {
        case EOO:
            s << "EOO";
            break;
        case _bson::Date:
            s << "new Date(" << (long long)date() << ')';
            break;
        case RegEx: {
            s << "/" << regex() << '/';
            const char *p = regexFlags();
            if (p) s << p;
        }
            break;
        case NumberDouble:
            s.appendDoubleNice(number());
            break;
        case NumberLong:
            s << _numberLong();
            break;
        case NumberInt:
            s << _numberInt();
            break;
        case _bson::Bool:
            s << (boolean() ? "true" : "false");
            break;
        case Object:
            embeddedObject().toString(s, false, full, depth + 1);
            break;
        case _bson::Array:
            embeddedObject().toString(s, true, full, depth + 1);
            break;
        case Undefined:
            s << "undefined";
            break;
        case jstNULL:
            s << "null";
            break;
        case MaxKey:
            s << "MaxKey";
            break;
        case MinKey:
            s << "MinKey";
            break;
        case CodeWScope:
            s << "CodeWScope( "
                << codeWScopeCode() << ", " << codeWScopeObject().toString(false, full) << ")";
            break;
        case Code:
            if (!full &&  valuestrsize() > 80) {
                s.write(valuestr(), 70);
                s << "...";
            }
            else {
                s.write(valuestr(), valuestrsize() - 1);
            }
            break;
        case Symbol:
        case _bson::String:
            s << '"';
            if (!full &&  valuestrsize() > 160) {
                s.write(valuestr(), 150);
                s << "...\"";
            }
            else {
                s.write(valuestr(), valuestrsize() - 1);
                s << '"';
            }
            break;
        case DBRef:
            s << "DBRef('" << valuestr() << "',";
            {
                _bson::OID *x = (_bson::OID *) (valuestr() + valuestrsize());
                s << x->toString() << ')';
            }
            break;
        case jstOID:
            s << "ObjectId('";
            s << __oid().toString() << "')";
            break;
        case BinData:
            s << "BinData(" << binDataType() << ", ";
            {
                int len;
                const char *data = binDataClean(len);
                if (!full && len > 80) {
                    s << toHex(data, 70) << "...)";
                }
                else {
                    s << toHex(data, len) << ")";
                }
            }
            break;
        case Timestamp:
            s << "Timestamp " << timestampTime() << "|" << timestampInc();
            break;
        default:
            s << "?type=" << type();
            break;
        }
    }

    /* return has eoo() true if no match
    supports "." notation to reach into embedded objects
    */
    BSONElement bsonobj::getFieldDotted(const StringData& name) const {
        BSONElement e = getField(name);
        if (e.eoo()) {
            size_t dot_offset = name.find('.');
            if (dot_offset != std::string::npos) {
                StringData left = name.substr(0, dot_offset);
                StringData right = name.substr(dot_offset + 1);
                bsonobj sub = getObjectField(left);
                return sub.isEmpty() ? BSONElement() : sub.getFieldDotted(right);
            }
        }

        return e;
    }

    void bsonobj::toString(StringBuilder& s, bool isArray, bool full, int depth) const {
        if (isEmpty()) {
            s << (isArray ? "[]" : "{}");
            return;
        }

        s << (isArray ? "[ " : "{ ");
        BSONObjIterator i(*this);
        bool first = true;
        while (1) {
            massert(10327, "Object does not end with EOO", i.moreWithEOO());
            BSONElement e = i.next(true);
            massert(10328, "Invalid element size", e.size() > 0);
            massert(10329, "Element too large", e.size() < (1 << 30));
            int offset = (int)(e.rawdata() - this->objdata());
            massert(10330, "Element extends past end of object",
                e.size() + offset <= this->objsize());
            bool end = (e.size() + offset == this->objsize());
            if (e.eoo()) {
                massert(10331, "EOO Before end of object", end);
                break;
            }
            if (first)
                first = false;
            else
                s << ", ";
            e.toString(s, !isArray, full, depth);
        }
        s << (isArray ? " ]" : " }");
    }
    bsonobj bsonobj::getObjectField(const StringData& name) const {
        BSONElement e = getField(name);
        BSONType t = e.type();
        return t == Object || t == Array ? e.embeddedObject() : bsonobj();
    }
    bsonobj::bsonobj() {
        /* little endian ordering here, but perhaps that is ok regardless as BSON is spec'd
        to be little endian external to the system. (i.e. the rest of the implementation of bson,
        not this part, fails to support big endian)
        */
        static char p[] = { /*size*/5, 0, 0, 0, /*eoo*/0 };
        _objdata = p;
    }
    BSONElement bsonobj::getField(const StringData& name) const {
        BSONObjIterator i(*this);
        while (i.more()) {
            BSONElement e = i.next();
            if (name == e.fieldName())
                return e;
        }
        return BSONElement();
    }
}
