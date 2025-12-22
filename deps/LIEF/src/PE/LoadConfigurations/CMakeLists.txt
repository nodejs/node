target_sources(LIB_LIEF PRIVATE
  EnclaveConfiguration.cpp
  EnclaveImport.cpp
  LoadConfiguration.cpp
  VolatileMetadata.cpp
)

add_subdirectory(CHPEMetadata)
add_subdirectory(DynamicRelocation)
