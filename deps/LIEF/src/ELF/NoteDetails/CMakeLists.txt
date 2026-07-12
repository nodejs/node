target_sources(LIB_LIEF PRIVATE
  NoteAbi.cpp
  AndroidIdent.cpp
  NoteGnuProperty.cpp
  QNXStack.cpp
)

add_subdirectory(core)
add_subdirectory(properties)
