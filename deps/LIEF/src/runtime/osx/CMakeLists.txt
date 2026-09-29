target_sources(LIB_LIEF_RUNTIME PRIVATE
  Host.cpp
  Memory.cpp
  Module.cpp
  Process.cpp
)

# objc
target_sources(LIB_LIEF_RUNTIME PRIVATE
  Host_utils.mm
  utils.mm
)

target_link_libraries(LIB_LIEF PRIVATE objc "-framework Foundation")
