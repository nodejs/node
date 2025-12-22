target_sources(LIB_LIEF PRIVATE
  Parser.cpp
  Parser.tcc
  File.cpp
  Header.cpp
  Header.tcc
  utils.cpp
  hash.cpp
  json_api.cpp
)

if(LIEF_ENABLE_JSON)
  target_sources(LIB_LIEF PRIVATE json.cpp)
endif()
