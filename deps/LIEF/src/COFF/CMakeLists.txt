target_sources(LIB_LIEF PRIVATE
  Binary.cpp
  utils.cpp
  Parser.cpp
  Header.cpp
  Section.cpp
  Relocation.cpp
  BigObjHeader.cpp
  RegularHeader.cpp
  Symbol.cpp
  AuxiliarySymbol.cpp
)

add_subdirectory(AuxiliarySymbols)
