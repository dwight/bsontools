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

#include <memory>
#include <sstream>
#include "json.h"
#include "string_data.h"
#include "errorcodes.h"
#include "status.h"
#include "base64.h"
#include "bsonobjbuilder.h"
#include "hex.h"
#include "parse_number.h"

using namespace std;

namespace _bson {

    using std::ostringstream;
    using std::string;

    unsigned long long line = 1;
    unsigned long long doc_number = 1;

#if 0
#define MONGO_JSON_DEBUG(message) log() << "JSON DEBUG @ " << __FILE__\
    << ":" << __LINE__ << " " << __FUNCTION__ << ": " << message << endl;
#else
#define MONGO_JSON_DEBUG(message)
#endif

#define ALPHA "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
#define DIGIT "0123456789"
#define CONTROL "\a\b\f\n\r\t\v"
#define JOPTIONS "gims"

    const char *DigitsSigned = "0123456789-";

    // Size hints given to char vectors
    enum {
        ID_RESERVE_SIZE = 64,
        PAT_RESERVE_SIZE = 4096,
        OPT_RESERVE_SIZE = 64,
        FIELD_RESERVE_SIZE = 4096,
        STRINGVAL_RESERVE_SIZE = 4096,
        BINDATA_RESERVE_SIZE = 4096,
        BINDATATYPE_RESERVE_SIZE = 4096,
        NS_RESERVE_SIZE = 64,
        DB_RESERVE_SIZE = 64,
        NUMBERLONG_RESERVE_SIZE = 64,
        DATE_RESERVE_SIZE = 64
    };

    static const char* LBRACE = "{",
                 *RBRACE = "}",
                 *LBRACKET = "[",
                 *RBRACKET = "]",
                 *LPAREN = "(",
                 *RPAREN = ")",
                 *COLON = ":",
                 *COMMA = ",",
                 *FORWARDSLASH = "/",
                 *SINGLEQUOTE = "'",
                 *DOUBLEQUOTE = "\"";

    JParse::JParse(istream& i) : _in(i), _offset(0) {}
//        : _buf(str), _input(str), _input_end(str + strlen(str)) {}

    Status JParse::parseError(const StringData& msg) {
        std::ostringstream ossmsg;
        ossmsg << msg.toString();
        ossmsg << " line:" << line;
        ossmsg << ", file_offset:" << offset() << ", doc_number:" << doc_number;
        return Status(ErrorCodes::FailedToParse, ossmsg.str());
    }

    Status JParse::err() { 
        return parseError("can't parse");
    }

    Status JParse::value(const StringData& fieldName, BSONObjBuilder& builder) {
        MONGO_JSON_DEBUG("fieldName: " << fieldName);
        char ch = peek();
        if (peekToken(LBRACE)) {
            Status ret = object(fieldName, builder);
            if (ret != Status::OK()) {
                return ret;
            }
        }
        else if (peekToken(LBRACKET)) {
            Status ret = array(fieldName, builder);
            if (ret != Status::OK()) {
                return ret;
            }
        }
        else if (ch == 'n') {
            getc();
            if (peek() == 'e' && readToken("ew")) {
                Status ret = constructor(fieldName, builder);
                if (ret != Status::OK()) {
                    return ret;
                }
            }
            else if (peek() == 'u' && readToken("ull")) {
                builder.appendNull(fieldName);
            }
            else
                return err();
        }
        else if (ch == 'D') {
            if (readToken("Date")) {
                Status ret = date(fieldName, builder);
                if (ret != Status::OK()) {
                    return ret;
                }
            }
            else
                return err();
        }
        else if (ch == 'T') {
            if (readToken("Timestamp")) {
                Status ret = timestamp(fieldName, builder);
                if (ret != Status::OK()) {
                    return ret;
                }
            }
            else
                return err();
        }
        else if (ch == 'O') {
            if (readToken("ObjectId")) {
                Status ret = objectId(fieldName, builder);
                if (ret != Status::OK()) {
                    return ret;
                }
            }
            else
                return err();
        }
        else if (ch == 'N') {
            if (readToken("NaN")) {
                builder.append(fieldName, std::numeric_limits<double>::quiet_NaN());
            }
            else if (readToken("NumberLong")) {
                Status ret = numberLong(fieldName, builder);
                if (ret != Status::OK()) {
                    return ret;
                }
            }
            else if (readToken("NumberInt")) {
                Status ret = numberInt(fieldName, builder);
                if (ret != Status::OK()) {
                    return ret;
                }
            }
            else
                return err();
        }
        else if (ch == 'D' && readToken("DBRef")) {
            Status ret = dbRef(fieldName, builder);
            if (ret != Status::OK()) {
                return ret;
            }
        }
        else if (peekToken(FORWARDSLASH)) {
            Status ret = regex(fieldName, builder);
            if (ret != Status::OK()) {
                return ret;
            }
        }
        else if (peekToken(DOUBLEQUOTE) || peekToken(SINGLEQUOTE)) {
            std::string valueString;
            valueString.reserve(STRINGVAL_RESERVE_SIZE);
            Status ret = quotedString(&valueString);
            if (ret != Status::OK()) {
                return ret;
            }
            builder.append(fieldName, valueString);
        }
        else if (ch == 't' && readToken("true")) {
            builder.append(fieldName, true);
        }
        else if (ch == 'f' && readToken("false")) {
            builder.append(fieldName, false);
        }
        else if (ch == 'u' && readToken("undefined")) {
            builder.appendUndefined(fieldName);
        }
        else if (ch== 'I' && readToken("Infinity")) {
            builder.append(fieldName, std::numeric_limits<double>::infinity());
        }
        else if (ch == '-' && readToken("-Infinity")) {
            builder.append(fieldName, -std::numeric_limits<double>::infinity());
        }
        else {
            Status ret = number(fieldName, builder);
            if (ret != Status::OK()) {
                return ret;
            }
        }
        return Status::OK();
    }

    Status JParse::object(const StringData& fieldName, BSONObjBuilder& builder, bool subObject) {
        MONGO_JSON_DEBUG("fieldName: " << fieldName);
        if (!readToken(LBRACE)) {
            return parseError("Expected '{'");
        }

        // Empty object
        if (peekToken(RBRACE)) {
            readToken(RBRACE);
            if (subObject) {
                BSONObjBuilder empty(builder.subobjStart(fieldName));
                empty.obj();
            }
            return Status::OK();
        }

        // Special object
        std::string firstField;
        firstField.reserve(FIELD_RESERVE_SIZE);
        Status ret = field(&firstField);
        if (ret != Status::OK()) {
            return ret;
        }

        bool reserved = false;

        if (*firstField.c_str() == '$') {
            if (firstField == "$oid") {
                reserved = true;
                if (!subObject) {
                    return parseError("Reserved field name in base object: $oid");
                }
                Status ret = objectIdObject(fieldName, builder);
                if (ret != Status::OK()) {
                    return ret;
                }
            }
            else if (firstField == "$binary") {
                reserved = true;
                if (!subObject) {
                    return parseError("Reserved field name in base object: $binary");
                }
                Status ret = binaryObject(fieldName, builder);
                if (ret != Status::OK()) {
                    return ret;
                }
            }
            else if (firstField == "$date") {
                reserved = true;
                if (!subObject) {
                    return parseError("Reserved field name in base object: $date");
                }
                Status ret = dateObject(fieldName, builder);
                if (ret != Status::OK()) {
                    return ret;
                }
            }
            else if (firstField == "$timestamp") {
                reserved = true;
                if (!subObject) {
                    return parseError("Reserved field name in base object: $timestamp");
                }
                Status ret = timestampObject(fieldName, builder);
                if (ret != Status::OK()) {
                    return ret;
                }
            }
            else if (firstField == "$regex") {
                reserved = true;
                if (!subObject) {
                    return parseError("Reserved field name in base object: $regex");
                }
                Status ret = regexObject(fieldName, builder);
                if (ret != Status::OK()) {
                    return ret;
                }
            }
            else if (firstField == "$ref") {
                reserved = true;
                if (!subObject) {
                    return parseError("Reserved field name in base object: $ref");
                }
                Status ret = dbRefObject(fieldName, builder);
                if (ret != Status::OK()) {
                    return ret;
                }
            }
            else if (firstField == "$undefined") {
                reserved = true;
                if (!subObject) {
                    return parseError("Reserved field name in base object: $undefined");
                }
                Status ret = undefinedObject(fieldName, builder);
                if (ret != Status::OK()) {
                    return ret;
                }
            }
            else if (firstField == "$numberLong") {
                reserved = true;
                if (!subObject) {
                    return parseError("Reserved field name in base object: $numberLong");
                }
                Status ret = numberLongObject(fieldName, builder);
                if (ret != Status::OK()) {
                    return ret;
                }
            }
        }

        if(!reserved) { // firstField != <reserved field name>
            // Normal object

            // Only create a sub builder if this is not the base object
            BSONObjBuilder* objBuilder = &builder;
            unique_ptr<BSONObjBuilder> subObjBuilder;
            if (subObject) {
                subObjBuilder.reset(new BSONObjBuilder(builder.subobjStart(fieldName)));
                objBuilder = subObjBuilder.get();
            }

            if (!readToken(COLON)) {
                return parseError("Expected ':'");
            }
            Status valueRet = value(firstField, *objBuilder);
            if (valueRet != Status::OK()) {
                return valueRet;
            }
            while (peekToken(COMMA)) {
                readToken(COMMA);
                std::string fieldName;
                fieldName.reserve(FIELD_RESERVE_SIZE);
                Status fieldRet = field(&fieldName);
                if (fieldRet != Status::OK()) {
                    return fieldRet;
                }
                if (!readToken(COLON)) {
                    return parseError("Expected ':'");
                }
                Status valueRet = value(fieldName, *objBuilder);
                if (valueRet != Status::OK()) {
                    return valueRet;
                }
            }
        }
        if (!readToken(RBRACE)) {
            return parseError("Expected '}' or ','");
        }
        return Status::OK();
    }

    Status JParse::objectIdObject(const StringData& fieldName, BSONObjBuilder& builder) {
        if (!readToken(COLON)) {
            return parseError("Expected ':'");
        }
        std::string id;
        id.reserve(ID_RESERVE_SIZE);
        Status ret = quotedString(&id);
        if (ret != Status::OK()) {
            return ret;
        }
        if (id.size() != 24) {
            return parseError("Expected 24 hex digits: " + id);
        }
        if (!isHexString(id)) {
            return parseError("Expected hex digits: " + id);
        }
        builder.append(fieldName, OID(id));
        return Status::OK();
    }

    Status JParse::binaryObject(const StringData& fieldName, BSONObjBuilder& builder) {
        if (!readToken(COLON)) {
            return parseError("Expected ':'");
        }
        std::string binDataString;
        binDataString.reserve(BINDATA_RESERVE_SIZE);
        Status dataRet = quotedString(&binDataString);
        if (dataRet != Status::OK()) {
            return dataRet;
        }
        if (binDataString.size() % 4 != 0) {
            return parseError("Invalid length base64 encoded string");
        }
        if (!isBase64String(binDataString)) {
            return parseError("Invalid character in base64 encoded string");
        }
        const std::string& binData = base64::decode(binDataString);
        if (!readToken(COMMA)) {
            return parseError("Expected ','");
        }

        if (!readField("$type")) {
            return parseError("Expected second field name: \"$type\", in \"$binary\" object");
        }
        if (!readToken(COLON)) {
            return parseError("Expected ':'");
        }
        std::string binDataType;
        binDataType.reserve(BINDATATYPE_RESERVE_SIZE);
        Status typeRet = quotedString(&binDataType);
        if (typeRet != Status::OK()) {
            return typeRet;
        }
        if ((binDataType.size() != 2) || !isHexString(binDataType)) {
            return parseError("Argument of $type in $bindata object must be a hex string representation of a single byte");
        }
        builder.appendBinData( fieldName, binData.length(),
                BinDataType(fromHex(binDataType)),
                binData.data());
        return Status::OK();
    }

    Status JParse::dateObject(const StringData& fieldName, BSONObjBuilder& builder) {
        if (!readToken(COLON)) {
            return parseError("Expected ':'");
        }
        errno = 0;
        Date_t date;

        if (peekToken(DOUBLEQUOTE)) {
            std::string dateString;
            dateString.reserve(DATE_RESERVE_SIZE);
            Status ret = quotedString(&dateString);
            if (!ret.isOK()) {
                return ret;
            }
            StatusWith<Date_t> dateRet = dateFromISOString(dateString);
            if (!dateRet.isOK()) {
                return dateRet.getStatus();
            }
            date = dateRet.getValue();
        }
        else if (readToken(LBRACE)) {
            std::string fieldName;
            fieldName.reserve(FIELD_RESERVE_SIZE);
            Status ret = field(&fieldName);
            if (ret != Status::OK()) {
                return ret;
            }
            if (fieldName != "$numberLong") {
                return parseError("Expected field name: $numberLong for $date value object");
            }
            if (!readToken(COLON)) {
                return parseError("Expected ':'");
            }

            // The number must be a quoted string, since large long numbers could overflow a double
            // and thus may not be valid JSON
            std::string numberLongString;
            numberLongString.reserve(NUMBERLONG_RESERVE_SIZE);
            ret = quotedString(&numberLongString);
            if (!ret.isOK()) {
                return ret;
            }

            long long numberLong;
            ret = parseNumberFromString(numberLongString, &numberLong);
            if (!ret.isOK()) {
                return ret;
            }
            date = numberLong;
        }
        else {
            // SERVER-11920: We should use parseNumberFromString here, but that function requires
            // that we know ahead of time where the number ends, which is not currently the case.
            string s = get(DigitsSigned);
            if (s.empty()) {
                return parseError("Date expecting integer milliseconds");
            }
            date = static_cast<unsigned long long>(strtoll(s.c_str(), NULL, 10));
            if (errno == ERANGE) {
                /* Need to handle this because jsonString outputs the value of Date_t as unsigned.
                * See SERVER-8330 and SERVER-8573 */
                errno = 0;
                // SERVER-11920: We should use parseNumberFromString here, but that function
                // requires that we know ahead of time where the number ends, which is not currently
                // the case.
                date = strtoull(s.c_str(), NULL, 10);
                if (errno == ERANGE) {
                    return parseError("Date milliseconds overflow");
                }
            }
        }
        builder.appendDate(fieldName, date);
        return Status::OK();
    }

    Status JParse::timestampObject(const StringData& fieldName, BSONObjBuilder& builder) {
        if (!readToken(COLON)) {
            return parseError("Expected ':'");
        }
        if (!readToken(LBRACE)) {
            return parseError("Expected '{' to start \"$timestamp\" object");
        }

        if (!readField("t")) {
            return parseError("Expected field name \"t\" in \"$timestamp\" sub object");
        }
        if (!readToken(COLON)) {
            return parseError("Expected ':'");
        }
        if (readToken("-")) {
            return parseError("Negative seconds in \"$timestamp\"");
        }
        errno = 0;
        // SERVER-11920: We should use parseNumberFromString here, but that function requires that
        // we know ahead of time where the number ends, which is not currently the case.
        string s = get(DIGIT);
        if (s.empty()) {
            return parseError("Expected unsigned integer seconds in \"$timestamp\"");
        }
        uint32_t seconds = strtoul(s.c_str(), NULL, 10);
        if (errno == ERANGE) {
            return parseError("Timestamp seconds overflow");
        }
        if (!readToken(COMMA)) {
            return parseError("Expected ','");
        }
        if (!readField("i")) {
            return parseError("Expected field name \"i\" in \"$timestamp\" sub object");
        }
        if (!readToken(COLON)) {
            return parseError("Expected ':'");
        }
        if (readToken("-")) {
            return parseError("Negative increment in \"$timestamp\"");
        }
        errno = 0;
        // SERVER-11920: We should use parseNumberFromString here, but that function requires that
        // we know ahead of time where the number ends, which is not currently the case.
        s = get(DIGIT);
        if (s.empty()) {
            return parseError("Expected unsigned integer increment in \"$timestamp\"");
        }
        uint32_t count = strtoul(s.c_str(), NULL, 10);
        if (errno == ERANGE) {
            return parseError("Timestamp increment overflow");
        }
        if (!readToken(RBRACE)) {
            return parseError("Expected '}'");
        }
        assert(false);
        //builder.appendTimestamp(fieldName, (static_cast<uint64_t>(seconds))*1000, count);
        return Status::OK();
    }

    Status JParse::regexObject(const StringData& fieldName, BSONObjBuilder& builder) {
        if (!readToken(COLON)) {
            return parseError("Expected ':'");
        }
        std::string pat;
        pat.reserve(PAT_RESERVE_SIZE);
        Status patRet = quotedString(&pat);
        if (patRet != Status::OK()) {
            return patRet;
        }
        if (readToken(COMMA)) {
            if (!readField("$options")) {
                return parseError("Expected field name: \"$options\" in \"$regex\" object");
            }
            if (!readToken(COLON)) {
                return parseError("Expected ':'");
            }
            std::string opt;
            opt.reserve(OPT_RESERVE_SIZE);
            Status optRet = quotedString(&opt);
            if (optRet != Status::OK()) {
                return optRet;
            }
            Status optCheckRet = regexOptCheck(opt);
            if (optCheckRet != Status::OK()) {
                return optCheckRet;
            }
            builder.appendRegex(fieldName, pat, opt);
        }
        else {
            builder.appendRegex(fieldName, pat, "");
        }
        return Status::OK();
    }

    Status JParse::dbRefObject(const StringData& fieldName, BSONObjBuilder& builder) {

        BSONObjBuilder subBuilder(builder.subobjStart(fieldName));

        if (!readToken(COLON)) {
            return parseError("DBRef: Expected ':'");
        }
        std::string ns;
        ns.reserve(NS_RESERVE_SIZE);
        Status ret = quotedString(&ns);
        if (ret != Status::OK()) {
            return ret;
        }
        subBuilder.append("$ref", ns);

        if (!readToken(COMMA)) {
            return parseError("DBRef: Expected ','");
        }

        if (!readField("$id")) {
            return parseError("DBRef: Expected field name: \"$id\" in \"$ref\" object");
        }
        if (!readToken(COLON)) {
            return parseError("DBRef: Expected ':'");
        }
        Status valueRet = value("$id", subBuilder);
        if (valueRet != Status::OK()) {
            return valueRet;
        }

        if (readToken(COMMA)) {
            if (!readField("$db")) {
                return parseError("DBRef: Expected field name: \"$db\" in \"$ref\" object");
            }
            if (!readToken(COLON)) {
                return parseError("DBRef: Expected ':'");
            }
            std::string db;
            db.reserve(DB_RESERVE_SIZE);
            ret = quotedString(&db);
            if (ret != Status::OK()) {
                return ret;
            }
            subBuilder.append("$db", db);
        }

        subBuilder.obj();
        return Status::OK();
    }

    Status JParse::undefinedObject(const StringData& fieldName, BSONObjBuilder& builder) {
        if (!readToken(COLON)) {
            return parseError("Expected ':'");
        }
        if (!readToken("true")) {
            return parseError("Reserved field \"$undefined\" requires value of true");
        }
        builder.appendUndefined(fieldName);
        return Status::OK();
    }

    Status JParse::numberLongObject(const StringData& fieldName, BSONObjBuilder& builder) {
        if (!readToken(COLON)) {
            return parseError("Expected ':'");
        }

        // The number must be a quoted string, since large long numbers could overflow a double and
        // thus may not be valid JSON
        std::string numberLongString;
        numberLongString.reserve(NUMBERLONG_RESERVE_SIZE);
        Status ret = quotedString(&numberLongString);
        if (!ret.isOK()) {
            return ret;
        }

        long long numberLong;
        ret = parseNumberFromString(numberLongString, &numberLong);
        if (!ret.isOK()) {
            return ret;
        }

        builder.appendNumber(fieldName, numberLong);
        return Status::OK();
    }

    Status JParse::array(const StringData& fieldName, BSONObjBuilder& builder) {
        MONGO_JSON_DEBUG("fieldName: " << fieldName);
        uint32_t index(0);
        if (!readToken(LBRACKET)) {
            return parseError("Expected '['");
        }
        BSONObjBuilder subBuilder(builder.subarrayStart(fieldName));
        if (!peekToken(RBRACKET)) {
            do {
                Status ret = value(builder.numStr(index), subBuilder);
                if (ret != Status::OK()) {
                    return ret;
                }
                index++;
            } while (readToken(COMMA));
        }
        subBuilder.obj();
        if (!readToken(RBRACKET)) {
            return parseError("Expected ']' or ','");
        }
        return Status::OK();
    }

    /* NOTE: this could be easily modified to allow "new" before other
     * constructors, but for now it only allows "new" before Date().
     * Also note that unlike the interactive shell "Date(x)" and "new Date(x)"
     * have the same behavior.  XXX: this may not be desired. */
    Status JParse::constructor(const StringData& fieldName, BSONObjBuilder& builder) {
        if (readToken("Date")) {
            date(fieldName, builder);
        }
        else {
            return parseError("\"new\" keyword not followed by Date constructor");
        }
        return Status::OK();
    }

    Status JParse::date(const StringData& fieldName, BSONObjBuilder& builder) {
        if (!readToken(LPAREN)) {
            return parseError("Expected '('");
        }
        errno = 0;

        // SERVER-11920: We should use parseNumberFromString here, but that function requires that
        // we know ahead of time where the number ends, which is not currently the case.
        string s = get(DigitsSigned);

        if (s.empty()) {
            return parseError("Date expecting integer milliseconds");
        }
        Date_t date = static_cast<unsigned long long>(strtoll(s.c_str(), NULL, 10));
        if (errno == ERANGE) {
            /* Need to handle this because jsonString outputs the value of Date_t as unsigned.
            * See SERVER-8330 and SERVER-8573 */
            errno = 0;
            // SERVER-11920: We should use parseNumberFromString here, but that function requires
            // that we know ahead of time where the number ends, which is not currently the case.
            date = strtoull(s.c_str(), NULL, 10);
            if (errno == ERANGE) {
                return parseError("Date milliseconds overflow");
            }
        }
        if (!readToken(RPAREN)) {
            return parseError("Expected ')'");
        }
        builder.appendDate(fieldName, date);
        return Status::OK();
    }

    Status JParse::timestamp(const StringData& fieldName, BSONObjBuilder& builder) {
        if (!readToken(LPAREN)) {
            return parseError("Expected '('");
        }
        if (readToken("-")) {
            return parseError("Negative seconds in \"$timestamp\"");
        }
        errno = 0;
        // SERVER-11920: We should use parseNumberFromString here, but that function requires that
        // we know ahead of time where the number ends, which is not currently the case.
        string s = get(DIGIT);
        if (s.empty()) {
            return parseError("Expected unsigned integer seconds in \"$timestamp\"");
        }
        uint32_t seconds = strtoul(s.c_str(), NULL, 10);
        if (errno == ERANGE) {
            return parseError("Timestamp seconds overflow");
        }
        if (!readToken(COMMA)) {
            return parseError("Expected ','");
        }
        if (readToken("-")) {
            return parseError("Negative seconds in \"$timestamp\"");
        }
        errno = 0;
        // SERVER-11920: We should use parseNumberFromString here, but that function requires that
        // we know ahead of time where the number ends, which is not currently the case.
        s = get(DIGIT);
        if (s.empty()) {
            return parseError("Expected unsigned integer increment in \"$timestamp\"");
        }
        uint32_t count = strtoul(s.c_str(), NULL, 10);
        if (errno == ERANGE) {
            return parseError("Timestamp increment overflow");
        }
        if (!readToken(RPAREN)) {
            return parseError("Expected ')'");
        }
        assert(false);
        //builder.appendTimestamp(fieldName, (static_cast<uint64_t>(seconds)) * 1000, count);
        return Status::OK();
    }

    Status JParse::objectId(const StringData& fieldName, BSONObjBuilder& builder) {
        if (!readToken(LPAREN)) {
            return parseError("Expected '('");
        }
        std::string id;
        id.reserve(ID_RESERVE_SIZE);
        Status ret = quotedString(&id);
        if (ret != Status::OK()) {
            return ret;
        }
        if (!readToken(RPAREN)) {
            return parseError("Expected ')'");
        }
        if (id.size() != 24) {
            return parseError("Expected 24 hex digits: " + id);
        }
        if (!isHexString(id)) {
            return parseError("Expected hex digits: " + id);
        }
        builder.append(fieldName, OID(id));
        return Status::OK();
    }

    Status JParse::numberLong(const StringData& fieldName, BSONObjBuilder& builder) {
        if (!readToken(LPAREN)) {
            return parseError("Expected '('");
        }
        errno = 0;
        // SERVER-11920: We should use parseNumberFromString here, but that function requires that
        // we know ahead of time where the number ends, which is not currently the case.
        string s = get(DigitsSigned);
        if (s.empty()) {
            return parseError("Expected number in NumberLong");
        }
        int64_t val = strtoll(s.c_str(), NULL, 10);
        if (errno == ERANGE) {
            return parseError("NumberLong out of range");
        }
        if (!readToken(RPAREN)) {
            return parseError("Expected ')'");
        }
        builder.appendNumber(fieldName, static_cast<long long int>(val));
        return Status::OK();
    }

    Status JParse::numberInt(const StringData& fieldName, BSONObjBuilder& builder) {
        if (!readToken(LPAREN)) {
            return parseError("Expected '('");
        }
        errno = 0;
        // SERVER-11920: We should use parseNumberFromString here, but that function requires that
        // we know ahead of time where the number ends, which is not currently the case.
        string s = get(DigitsSigned);
        if (s.empty()) {
            return parseError("Expected unsigned number in NumberInt");
        }
        int32_t val = strtol(s.c_str(), NULL, 10);
        if (errno == ERANGE) {
            return parseError("NumberInt out of range");
        }
        if (!readToken(RPAREN)) {
            return parseError("Expected ')'");
        }
        builder.appendNumber(fieldName, static_cast<int>(val));
        return Status::OK();
    }


    Status JParse::dbRef(const StringData& fieldName, BSONObjBuilder& builder) {
        BSONObjBuilder subBuilder(builder.subobjStart(fieldName));

        if (!readToken(LPAREN)) {
            return parseError("Expected '('");
        }
        std::string ns;
        ns.reserve(NS_RESERVE_SIZE);
        Status refRet = quotedString(&ns);
        if (refRet != Status::OK()) {
            return refRet;
        }
        subBuilder.append("$ref", ns);

        if (!readToken(COMMA)) {
            return parseError("Expected ','");
        }

        Status valueRet = value("$id", subBuilder);
        if (valueRet != Status::OK()) {
            return valueRet;
        }

        if (readToken(COMMA)) {
            std::string db;
            db.reserve(DB_RESERVE_SIZE);
            Status dbRet = quotedString(&db);
            if (dbRet != Status::OK()) {
                return dbRet;
            }
            subBuilder.append("$db", db);
        }

        if (!readToken(RPAREN)) {
            return parseError("Expected ')'");
        }

        subBuilder.obj();
        return Status::OK();
    }

    Status JParse::regex(const StringData& fieldName, BSONObjBuilder& builder) {
        if (!readToken(FORWARDSLASH)) {
            return parseError("Expected '/'");
        }
        std::string pat;
        pat.reserve(PAT_RESERVE_SIZE);
        Status patRet = regexPat(&pat);
        if (patRet != Status::OK()) {
            return patRet;
        }
        if (!readToken(FORWARDSLASH)) {
            return parseError("Expected '/'");
        }
        std::string opt;
        opt.reserve(OPT_RESERVE_SIZE);
        Status optRet = regexOpt(&opt);
        if (optRet != Status::OK()) {
            return optRet;
        }
        Status optCheckRet = regexOptCheck(opt);
        if (optCheckRet != Status::OK()) {
            return optCheckRet;
        }
        builder.appendRegex(fieldName, pat, opt);
        return Status::OK();
    }

    Status JParse::regexPat(std::string* result) {
        MONGO_JSON_DEBUG("");
        return chars(result, "/");
    }

    Status JParse::regexOpt(std::string* result) {
        MONGO_JSON_DEBUG("");
        return chars(result, "", JOPTIONS);
    }

    Status JParse::regexOptCheck(const StringData& opt) {
        MONGO_JSON_DEBUG("opt: " << opt);
        std::size_t i;
        for (i = 0; i < opt.size(); i++) {
            if (!match(opt[i], JOPTIONS)) {
                return parseError(string("Bad regex option: ") + opt[i]);
            }
        }
        return Status::OK();
    }

    Status JParse::number(const StringData& fieldName, BSONObjBuilder& builder) {
        long long retll;
        double retd;

        // reset errno to make sure that we are getting it from strtod
        errno = 0;
        // SERVER-11920: We should use parseNumberFromString here, but that function requires that
        // we know ahead of time where the number ends, which is not currently the case.
        string s = get("0123456789-+Ee.");
        if (s.empty()) {
            return parseError("Bad characters in value");
        }
        retd = strtod(s.c_str(), NULL);
        if (errno == ERANGE) {
            return parseError("Value cannot fit in double");
        }
        // reset errno to make sure that we are getting it from strtoll
        errno = 0;
        // SERVER-11920: We should use parseNumberFromString here, but that function requires that
        // we know ahead of time where the number ends, which is not currently the case.
        retll = strtoll(s.c_str(), NULL, 10);
        if (errno == ERANGE) {
            // The number either had characters only meaningful for a double or
            // could not fit in a 64 bit int
            MONGO_JSON_DEBUG("Type: double");
            builder.append(fieldName, retd);
        }
        else if (retll == static_cast<int>(retll)) {
            // The number can fit in a 32 bit int
            MONGO_JSON_DEBUG("Type: 32 bit int");
            builder.append(fieldName, static_cast<int>(retll));
        }
        else {
            // The number can fit in a 64 bit int
            MONGO_JSON_DEBUG("Type: 64 bit int");
            builder.append(fieldName, retll);
        }
        return Status::OK();
    }

    Status JParse::field(std::string* result) {
        MONGO_JSON_DEBUG("");
        if (peekToken(DOUBLEQUOTE) || peekToken(SINGLEQUOTE)) {
            // Quoted key
            // TODO: make sure quoted field names cannot contain null characters
            return quotedString(result);
        }
        else {
            // Unquoted key
            // 'isspace()' takes an 'int' (signed), so (default signed) 'char's get sign-extended
            // and therefore 'corrupted' unless we force them to be unsigned ... 0x80 becomes
            // 0xffffff80 as seen by isspace when sign-extended ... we want it to be 0x00000080
            while (!eof() &&
                   isspace((unsigned char)peek())) {
                getc();
            }
            if (eof()) {
                return parseError("Field name expected");
            }
            if (!match(peek(), ALPHA "_$")) {
                return parseError("First character in field must be [A-Za-z$_]");
            }
            return chars(result, "", ALPHA DIGIT "_$");
        }
    }

    Status JParse::quotedString(std::string* result) {
        MONGO_JSON_DEBUG("");
        if (peekToken(DOUBLEQUOTE)) {
            getc();
            Status ret = chars(result, "\"");
            if (ret != Status::OK()) {
                return ret;
            }
            if (!readToken(DOUBLEQUOTE)) {
                return parseError("Expected '\"'");
            }
        }
        else if (peekToken(SINGLEQUOTE)) {
            getc();
            Status ret = chars(result, "'");
            if (ret != Status::OK()) {
                return ret;
            }
            if (!readToken(SINGLEQUOTE)) {
                return parseError("Expected '''");
            }
        }
        else {
            return parseError("Expected quoted string");
        }
        return Status::OK();
    }

    /*
     * terminalSet are characters that signal end of string (e.g.) [ :\0]
     * allowedSet are the characters that are allowed, if this is set
     */
    Status JParse::chars(std::string* result, const char* terminalSet,
            const char* allowedSet) {
        MONGO_JSON_DEBUG("terminalSet: " << terminalSet);
        if (eof()) {
            return parseError("Unexpected end of input");
        }
        while (1) {
            if (eof())
                break;
            char ch = peek();
            if (match(ch, terminalSet))
                break;
            MONGO_JSON_DEBUG("q: " << ch);
            if (allowedSet != NULL) {
                if (!match(ch, allowedSet)) {
                    return Status::OK();
                }
            }
            if (0x00 <= ch && ch <= 0x1F) {
                return parseError("Invalid control character");
            }
            getc();
            if (ch == '\\' && !eof()) {
                ch = peek();
                switch (ch) {
                    // Escape characters allowed by the JSON spec
                    case '"':  result->push_back('"');  break;
                    case '\'': result->push_back('\''); break;
                    case '\\': result->push_back('\\'); break;
                    case '/':  result->push_back('/');  break;
                    case 'b':  result->push_back('\b'); break;
                    case 'f':  result->push_back('\f'); break;
                    case 'n':  result->push_back('\n'); break;
                    case 'r':  result->push_back('\r'); break;
                    case 't':  result->push_back('\t'); break;
                    case 'u': { //expect 4 hexdigits
                        // TODO: handle UTF-16 surrogate characters
                        getc();
                        stringstream ss;
                        ss << getc() << getc() << getc() << peek();
                        string s = ss.str();
                            if (!isHexString(s)) {
                                return parseError("Expected 4 hex digits");
                            }
                            unsigned char first = fromHex(s);
                            unsigned char second = fromHex(s.c_str() + 2);
                            const std::string& utf8str = encodeUTF8(first, second);
                            for (unsigned int i = 0; i < utf8str.size(); i++) {
                                result->push_back(utf8str[i]);
                            }
                            break;
                        }
                    // Vertical tab character.  Not in JSON spec but allowed in
                    // our implementation according to test suite.
                    case 'v':  result->push_back('\v'); break;
                               // Escape characters we explicity disallow
                    case 'x':  return parseError("Hex escape not supported");
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':  return parseError("Octal escape not supported");
                               // By default pass on the unescaped character
                    default:   result->push_back(ch); break;
                    // TODO: check for escaped control characters
                }
                getc();
            }
            else {
                result->push_back(ch);
            }
        }
        if (!eof()) {
            return Status::OK();
        }
        return parseError("Unexpected end of input");
    }

    std::string JParse::encodeUTF8(unsigned char first, unsigned char second) const {
        std::ostringstream oss;
        if (first == 0 && second < 0x80) {
            oss << second;
        }
        else if (first < 0x08) {
            oss << char( 0xc0 | (first << 2 | second >> 6) );
            oss << char( 0x80 | (~0xc0 & second) );
        }
        else {
            oss << char( 0xe0 | (first >> 4) );
            oss << char( 0x80 | (~0xc0 & (first << 2 | second >> 6) ) );
            oss << char( 0x80 | (~0xc0 & second) );
        }
        return oss.str();
    }

    inline bool JParse::peekToken(const char* token) {
        assert(*token);
        assert(token[1] == 0);
        return peek() == *token;
    }

    inline bool JParse::readToken(const char* token) {
        return readTokenImpl(token);
    }

    string JParse::get(const char *chars_wanted) { 
        StringBuilder b;
        while (1) {
            char ch = peek();
            if (strchr(chars_wanted, ch) == 0)
                break;
            getc();
            b << ch;
        }
        return b.str();
    }

    char JParse::getc() { 
        _offset++;
        char ch = _in.get(); 
        if (ch == '\n')
            line++;
        return ch;
    }

    char JParse::peek() { 
        return _in.peek();
    }

    bool JParse::readTokenImpl(const char* token) {
        MONGO_JSON_DEBUG("token: " << token);
        if (token == NULL) {
            return false;
        }
        // 'isspace()' takes an 'int' (signed), so (default signed) 'char's get sign-extended
        // and therefore 'corrupted' unless we force them to be unsigned ... 0x80 becomes
        // 0xffffff80 as seen by isspace when sign-extended ... we want it to be 0x00000080

        while (!eof() && isspace((unsigned char) peek())) {
            getc();
        }
        while (*token != '\0') {
            if (eof()) {
                return false;
            }
            if (*token++ != getc()) {
                return false;
            }
        }
        return true;
    }

    bool JParse::readField(const StringData& expectedField) {
        MONGO_JSON_DEBUG("expectedField: " << expectedField);
        std::string nextField;
        nextField.reserve(FIELD_RESERVE_SIZE);
        Status ret = field(&nextField);
        if (ret != Status::OK()) {
            return false;
        }
        if (expectedField != nextField) {
            return false;
        }
        return true;
    }

    inline bool JParse::match(char matchChar, const char* matchSet) const {
        if (matchSet == NULL) {
            return true;
        }
        if (*matchSet == '\0') {
            return false;
        }
        return (strchr(matchSet, matchChar) != NULL);
    }

    bool JParse::isHexString(const StringData& str) const {
        MONGO_JSON_DEBUG("str: " << str);
        std::size_t i;
        for (i = 0; i < str.size(); i++) {
            if (!isxdigit(str[i])) {
                return false;
            }
        }
        return true;
    }

    bool JParse::isBase64String(const StringData& str) const {
        MONGO_JSON_DEBUG("str: " << str);
        std::size_t i;
        for (i = 0; i < str.size(); i++) {
            if (!match(str[i], base64::chars)) {
                return false;
            }
        }
        return true;
    }

    bsonobj fromjson(std::istream& i, BSONObjBuilder& builder) {
        if (i.eof()) {
            return bsonobj();
        }
        char z = i.peek();

        JParse jparse(i);
        Status ret = Status::OK();
        try {
            ret = jparse.object("UNUSED", builder, false);
        }
        catch(std::exception& e) {
            std::ostringstream message;
            message << "caught exception from within JSON parser: " << e.what();
            throw MsgAssertionException(17031, message.str());
        }

        if (ret != Status::OK()) {
            ostringstream message;
            message << "parse error - " << ret.codeString(); // << ": " << ret.reason();
            string s = message.str();
            throw MsgAssertionException(16619, s);
        }
        doc_number++;
        return builder.obj();
    }
    /*
    bsonobj fromjson(const std::string& str, BSONObjBuilder& b) {
        return fromjson( str.c_str(), b );
    }*/

}  /* namespace mongo */
