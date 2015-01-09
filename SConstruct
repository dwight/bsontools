env = Environment()

env.Append(CCFLAGS='-std=c++0x')

# env.Program(target = 'hex', source = ["src/bsontools/hex.cpp"])

env.Program(target = 'hex', source = ["src/bsontools/hex.cpp"])

dep1 = [
    "../bson-cxx/src/bson/time_support.cpp",
    "../bson-cxx/src/bson/bson.cpp",
    "../bson-cxx/src/bson/parse_number.cpp" ]

dep2 = [
    "../bson-cxx/src/bson/json.cpp",
    "../bson-cxx/src/bson/base64.cpp"
    ]

env.Program(target = 'fromcsv', source = ["src/bsontools/fromcsv.cpp"] + dep1)

env.Program(target = 'fromjson', source = ["src/bsontools/fromjson.cpp"] + dep1 + dep2)

env.Program(target = 'fromxml', source = ["src/bsontools/fromxml.cpp"] + dep1)

env.Program(target = 'bson', source = ["src/bsontools/bsonmain.cpp"] + dep1 + dep2)
