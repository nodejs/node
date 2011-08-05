#
# configure node for building
#
include(CheckFunctionExists)


if(NOT "v${CMAKE_BUILD_TYPE}" MATCHES vDebug)
  set(CMAKE_BUILD_TYPE "Release")
endif()

string(TOLOWER ${CMAKE_SYSTEM_NAME} node_platform)

if(${node_platform} MATCHES darwin)
  execute_process(COMMAND sw_vers -productVersion OUTPUT_VARIABLE OSX_VERSION)
  string(REGEX REPLACE "^([0-9]+\\.[0-9]+).*$" "\\1" OSX_VERSION "${OSX_VERSION}")

  if(OSX_VERSION GREATER 10.5)
    # 10.6 builds are 64-bit
    set(CMAKE_SYSTEM_PROCESSOR x86_64)
  endif()
endif()

# Get system architecture
if(${CMAKE_SYSTEM_PROCESSOR} MATCHES i686*)
  set(node_arch x86)
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES i386*)
  set(node_arch x86)
else()
  set(node_arch ${CMAKE_SYSTEM_PROCESSOR})
endif()

if(${node_arch} MATCHES unknown)
  set(node_arch x86)
endif()

set(NODE_INCLUDE_PREFIX ${CMAKE_INSTALL_PREFIX})

# Copy tools directory for out-of-source build
string(COMPARE EQUAL $(PROJECT_BINARY_DIR) ${PROJECT_SOURCE_DIR} in_source_build)
if(NOT in_source_build)
  execute_process(COMMAND cmake -E copy_directory ${PROJECT_SOURCE_DIR}/tools ${PROJECT_BINARY_DIR}/tools)
  configure_file(${PROJECT_SOURCE_DIR}/deps/v8/tools/jsmin.py ${PROJECT_BINARY_DIR}/tools COPYONLY)
endif()

# Set some compiler/linker flags..
set(CMAKE_C_FLAGS_DEBUG "-O0 -Wall -g -Wextra -DDEBUG $ENV{CFLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -Wall -g -Wextra -DDEBUG $ENV{CXXFLAGS}")

set(CMAKE_C_FLAGS_RELEASE "-g -O3 -DNDEBUG $ENV{CFLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "-g -O3 -DNDEBUG $ENV{CXXFLAGS}")

if(NOT ${node_platform} MATCHES windows)
  add_definitions(-D__POSIX__=1)
endif()

if(${node_platform} MATCHES sunos)
  add_definitions(-threads)
elseif(NOT ${node_platform} MATCHES cygwin*)
  add_definitions(-pthread)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -rdynamic")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -rdynamic")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")
endif()

if(${node_platform} MATCHES darwin)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework Carbon")
  # explicitly set this so that we don't check again when building libeio
  set(HAVE_FDATASYNC 0)
else()
  # OSX fdatasync() check wrong: http://public.kitware.com/Bug/view.php?id=10044
  check_function_exists(fdatasync HAVE_FDATASYNC)
endif()

if(HAVE_FDATASYNC)
  add_definitions(-DHAVE_FDATASYNC=1)
else()
  add_definitions(-DHAVE_FDATASYNC=0)
endif()

if(DTRACE)
  if(NOT ${node_platform} MATCHES sunos)
    message(FATAL_ERROR "DTrace support only currently available on Solaris")
  endif()
  find_program(dtrace_bin dtrace)
  if(NOT dtrace_bin)
    message(FATAL_ERROR "DTrace binary not found")
  endif()
  add_definitions(-DHAVE_DTRACE=1)
endif()

add_definitions(
  -DPLATFORM="${node_platform}"
  -DX_STACKSIZE=65536
  -D_LARGEFILE_SOURCE
  -D_FILE_OFFSET_BITS=64
  -DEV_MULTIPLICITY=0
  -D_FORTIFY_SOURCE=2
  )

# set the exec output path to be compatible with the current waf build system
if(${CMAKE_BUILD_TYPE} MATCHES Debug)
  set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/debug/)
else()
  set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/default/)
endif()

#
## ---------------------------------------------------------
#

file(GLOB js2c_files ${PROJECT_SOURCE_DIR}/lib/*.js)
set(js2c_files ${PROJECT_SOURCE_DIR}/src/node.js ${js2c_files})
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/src)

