target_sources(LIB_LIEF PRIVATE
  Binary.cpp
  Symbol.cpp
  Header.cpp
  Section.cpp
  Section.tcc
  Parser.cpp
  Relocation.cpp
  Function.cpp
  hash.cpp
  json_api.cpp)

if(LIEF_ENABLE_JSON)
  target_sources(LIB_LIEF PRIVATE json.cpp)
endif()


if(NOT LIEF_DEBUG_INFO_SUPPORT)
  target_sources(LIB_LIEF PRIVATE debug_info.cpp)
endif()
