target_sources(LIB_LIEF PRIVATE
  RuntimeFunctionX64.cpp
  RuntimeFunctionAArch64.cpp
  UnwindCodeX64.cpp
  UnwindAArch64Decoder.cpp
)

add_subdirectory(AArch64)
