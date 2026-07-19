target_sources(LIB_LIEF PRIVATE
  DynamicFixup.cpp
  DynamicFixupARM64Kernel.cpp
  DynamicFixupARM64X.cpp
  DynamicFixupControlTransfer.cpp
  DynamicFixupGeneric.cpp
  DynamicRelocationBase.cpp
  DynamicRelocationV1.cpp
  DynamicRelocationV2.cpp
  FunctionOverride.cpp
  FunctionOverrideInfo.cpp
)
