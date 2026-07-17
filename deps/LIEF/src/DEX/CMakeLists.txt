target_sources(LIB_LIEF PRIVATE
  Parser.cpp
  Parser.tcc
  File.cpp
  EnumToString.cpp
  Header.cpp
  Header.tcc
  Class.cpp
  Method.cpp
  Field.cpp
  instructions.cpp
  CodeInfo.cpp
  Type.cpp
  Prototype.cpp
  MapList.cpp
  MapItem.cpp
  utils.cpp
  hash.cpp
  json_api.cpp
)


if(LIEF_ENABLE_JSON)
  target_sources(LIB_LIEF PRIVATE json.cpp)
endif()
