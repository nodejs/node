if (LIEF_RUNTIME AND LIEF_ASM)
  target_sources(LIB_LIEF_RUNTIME PRIVATE
  )
else()
  target_sources(LIB_LIEF_RUNTIME PRIVATE
    no_disassembler.cpp
    no_assembler.cpp
  )
endif()
