// oid.h

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

#include "time_support.h"
#include "hex.h"

namespace _bson {

#pragma pack(1)

    /** Object ID type.
        BSON objects typically have an _id field for the object id.  This field should be the first
        member of the object when present.  class OID is a special type that is a 12 byte id which
        is likely to be unique to the system.  You may also use other types for _id's.
        When _id field is missing from a BSON object, on an insert the database may insert one
        automatically in certain circumstances.

        Warning: You must call OID::newState() after a fork().

        Typical contents of the BSON ObjectID is a 12-byte value consisting of a 4-byte timestamp (seconds since epoch),
        a 3-byte machine id, a 2-byte process id, and a 3-byte counter. Note that the timestamp and counter fields must
        be stored big endian unlike the rest of BSON. This is because they are compared byte-by-byte and we want to ensure
        a mostly increasing order.
    */
    class OID {
    public:
        OID() : a(0), b(0) { }

        enum {
            kOIDSize = 12,
            kIncSize = 3
        };

        /** init from a 24 char hex string */
        explicit OID(const std::string &s) { init(s); }

        /** init from a reference to a 12-byte array */
        explicit OID(const unsigned char (&arr)[kOIDSize]) {
            memcpy(data, arr, sizeof(arr));
        }

        /** initialize to 'null' */
        void clear() { a = 0; b = 0; }

        const unsigned char *getData() const { return data; }

        bool operator==(const OID& r) const { return a==r.a && b==r.b; }
        bool operator!=(const OID& r) const { return a!=r.a || b!=r.b; }
        int compare( const OID& other ) const { return memcmp( data , other.data , kOIDSize ); }
        bool operator<( const OID& other ) const { return compare( other ) < 0; }
        bool operator<=( const OID& other ) const { return compare( other ) <= 0; }

        /** @return the object ID output as 24 hex digits */
        std::string str() const;
        std::string toString() const { return str(); }
        /** @return the random/sequential part of the object ID as 6 hex digits */

        /** sets the contents to a new oid / randomized value */
        //void init();

        /** sets the contents to a new oid
         * guaranteed to be sequential
         * NOT guaranteed to be globally unique
         *     only unique for this process
         * */
        void initSequential();

        /** init from a 24 char hex string */
        void init(const std::string& s) {
            verify(s.size() == 24);
            const char *p = s.c_str();
            for (size_t i = 0; i < kOIDSize; i++) {
                data[i] = fromHex(p);
                p += 2;
            }
        }

        /** Set to the min/max OID that could be generated at given timestamp. */
        void init( Date_t date, bool max=false );

        time_t asTimeT();
        Date_t asDateT() { return asTimeT() * (long long)1000; }

        bool isSet() const { return a || b; }

        /**
         * this is not consistent
         * do not store on disk
         */
        void hash_combine(size_t &seed) const;

    private:
        struct MachineAndPid {
            unsigned char _machineNumber[3];
            unsigned short _pid;
            bool operator!=(const OID::MachineAndPid& rhs) const;
        };
        static MachineAndPid ourMachine;
        static MachineAndPid ourMachineAndPid;
        union {
            struct {
                // 12 bytes total
                unsigned char _time[4];
                MachineAndPid _machineAndPid;
                unsigned char _inc[3];
            };
            struct {
                long long a;
                unsigned b;
            };
            struct {
                // TODO: get rid of this eventually
                //       this is a hack because of hash_combine with older versions of boost
                //       on 32-bit platforms
                int x;
                int y;
                int z;
            };
            unsigned char data[kOIDSize];
        };

    };
#pragma pack()


}
