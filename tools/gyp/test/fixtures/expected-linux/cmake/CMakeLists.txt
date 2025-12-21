cmake_minimum_required(VERSION 2.8.8 FATAL_ERROR)
cmake_policy(VERSION 2.8.8)
project(test)
set(configuration "Default")
enable_language(ASM)
set(builddir "${CMAKE_CURRENT_BINARY_DIR}")
set(obj "${builddir}/obj")

set(CMAKE_C_OUTPUT_EXTENSION_REPLACE 1)
set(CMAKE_CXX_OUTPUT_EXTENSION_REPLACE 1)

set(CMAKE_NINJA_FORCE_RESPONSE_FILE 1)


#*/test/fixtures/integration.gyp:test#target
set(TARGET "test")
set(TOOLSET "target")
set(test__cxx_srcs "../../test.cc")
link_directories( ../../mylib
)
add_executable(test ${test__cxx_srcs})
set_target_properties(test PROPERTIES EXCLUDE_FROM_ALL "FALSE")
set_target_properties(test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${builddir}")
set_target_properties(test PROPERTIES PREFIX "")
set_target_properties(test PROPERTIES RUNTIME_OUTPUT_NAME "test")
set_target_properties(test PROPERTIES SUFFIX "")
set_source_files_properties(${builddir}/test PROPERTIES GENERATED "TRUE")
set(test__include_dirs "${CMAKE_CURRENT_LIST_DIR}/../../include")
set_property(TARGET test APPEND PROPERTY INCLUDE_DIRECTORIES ${test__include_dirs})
set_target_properties(test PROPERTIES COMPILE_FLAGS "")
unset(TOOLSET)
unset(TARGET)
