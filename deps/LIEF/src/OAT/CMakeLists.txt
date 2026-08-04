target_sources(LIB_LIEF PRIVATE
  Parser.cpp
  Parser.tcc
  Binary.cpp
  Header.cpp
  DexFile.cpp
  Class.cpp
  Method.cpp
  Header.tcc
  EnumToString.cpp
  utils.cpp
  hash.cpp
  oat_64.tcc
  oat_79.tcc
  oat_131.tcc
  oat_124.tcc
  json_api.cpp
)

if(LIEF_ENABLE_JSON)
  target_sources(LIB_LIEF PRIVATE json.cpp)
endif()

