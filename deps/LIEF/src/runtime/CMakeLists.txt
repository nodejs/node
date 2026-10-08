add_library(LIB_LIEF_RUNTIME OBJECT)

target_include_directories(LIB_LIEF_RUNTIME PRIVATE
  $<TARGET_PROPERTY:LIB_LIEF,INCLUDE_DIRECTORIES>
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(LIB_LIEF_RUNTIME PRIVATE
  $<TARGET_PROPERTY:LIB_LIEF,COMPILE_DEFINITIONS>)

target_compile_options(LIB_LIEF_RUNTIME PRIVATE
  $<TARGET_PROPERTY:LIB_LIEF,COMPILE_OPTIONS>)

target_link_libraries(LIB_LIEF_RUNTIME PRIVATE lief_spdlog)

set_target_properties(LIB_LIEF_RUNTIME PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  VISIBILITY_INLINES_HIDDEN ON
  CXX_VISIBILITY_PRESET     hidden
  C_VISIBILITY_PRESET       hidden)

target_compile_features(LIB_LIEF_RUNTIME PRIVATE cxx_std_23)

target_sources(LIB_LIEF PRIVATE $<TARGET_OBJECTS:LIB_LIEF_RUNTIME>)

target_sources(LIB_LIEF_RUNTIME PRIVATE
  osx/common.cpp
  windows/common.cpp
)

add_subdirectory(disassembler)

if (LIEF_RUNTIME)
  if (LIEF_RUNTIME_PLATFORM_LINUX OR LIEF_RUNTIME_PLATFORM_ANDROID)
    add_subdirectory(linux_android_shared)
  endif()

  if (LIEF_RUNTIME_PLATFORM_LINUX)
    add_subdirectory(linux)
    target_link_libraries(LIB_LIEF PUBLIC dl)
  else()
    target_sources(LIB_LIEF_RUNTIME PRIVATE linux/no_runtime.cpp)
  endif()

  if (LIEF_RUNTIME_PLATFORM_WINDOWS)
    add_subdirectory(windows)
  else()
    target_sources(LIB_LIEF_RUNTIME PRIVATE windows/no_runtime.cpp)
  endif()

  if (LIEF_RUNTIME_PLATFORM_ANDROID)
    add_subdirectory(android)
  else()
    target_sources(LIB_LIEF_RUNTIME PRIVATE android/no_runtime.cpp)
  endif()

  if (LIEF_RUNTIME_PLATFORM_OSX)
    add_subdirectory(osx)
  else()
    target_sources(LIB_LIEF_RUNTIME PRIVATE osx/no_runtime.cpp)
  endif()

else()
  target_sources(LIB_LIEF_RUNTIME PRIVATE
    no_runtime.cpp
    linux/no_runtime.cpp
    linux_android_shared/no_runtime.cpp
    windows/no_runtime.cpp
    android/no_runtime.cpp
    osx/no_runtime.cpp
  )
endif()
