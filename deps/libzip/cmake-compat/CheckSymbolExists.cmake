# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
CheckSymbolExists
-----------------

Provides a macro to check if a symbol exists as a function, variable,
or macro in ``C``.

.. command:: check_symbol_exists

  .. code-block:: cmake

    check_symbol_exists(<symbol> <files> <variable>)

  Check that the ``<symbol>`` is available after including given header
  ``<files>`` and store the result in a ``<variable>``.  Specify the list
  of files in one argument as a semicolon-separated list.
  ``<variable>`` will be created as an internal cache variable.

If the header files define the symbol as a macro it is considered
available and assumed to work.  If the header files declare the symbol
as a function or variable then the symbol must also be available for
linking (so intrinsics may not be detected).
If the symbol is a type, enum value, or intrinsic it will not be recognized
(consider using :module:`CheckTypeSize` or :module:`CheckCSourceCompiles`).
If the check needs to be done in C++, consider using
:module:`CheckCXXSymbolExists` instead.

The following variables may be set before calling this macro to modify
the way the check is run:

``CMAKE_REQUIRED_FLAGS``
  string of compile command line flags.
``CMAKE_REQUIRED_DEFINITIONS``
  a :ref:`;-list <CMake Language Lists>` of macros to define (-DFOO=bar).
``CMAKE_REQUIRED_INCLUDES``
  a :ref:`;-list <CMake Language Lists>` of header search paths to pass to
  the compiler.
``CMAKE_REQUIRED_LINK_OPTIONS``
  a :ref:`;-list <CMake Language Lists>` of options to add to the link command.
``CMAKE_REQUIRED_LIBRARIES``
  a :ref:`;-list <CMake Language Lists>` of libraries to add to the link
  command. See policy :policy:`CMP0075`.
``CMAKE_REQUIRED_QUIET``
  execute quietly without messages.

For example:

.. code-block:: cmake

  include(CheckSymbolExists)

  # Check for macro SEEK_SET
  check_symbol_exists(SEEK_SET "stdio.h" HAVE_SEEK_SET)
  # Check for function fopen
  check_symbol_exists(fopen "stdio.h" HAVE_FOPEN)
#]=======================================================================]

if(__CheckSymbolExists_cmake__)
  return()
endif()
set(__CheckSymbolExists_cmake__ TRUE)

cmake_policy(PUSH)
cmake_policy(SET CMP0054 NEW) # if() quoted variables not dereferenced

macro(CHECK_SYMBOL_EXISTS SYMBOL FILES VARIABLE)
  if(CMAKE_C_COMPILER_LOADED)
    __CHECK_SYMBOL_EXISTS_IMPL("${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/CheckSymbolExists.c" "${SYMBOL}" "${FILES}" "${VARIABLE}" )
  elseif(CMAKE_CXX_COMPILER_LOADED)
    __CHECK_SYMBOL_EXISTS_IMPL("${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/CheckSymbolExists.cxx" "${SYMBOL}" "${FILES}" "${VARIABLE}" )
  else()
    message(FATAL_ERROR "CHECK_SYMBOL_EXISTS needs either C or CXX language enabled")
  endif()
endmacro()

macro(__CHECK_SYMBOL_EXISTS_IMPL SOURCEFILE SYMBOL FILES VARIABLE)
  if(NOT DEFINED "${VARIABLE}" OR "x${${VARIABLE}}" STREQUAL "x${VARIABLE}")
    set(CMAKE_CONFIGURABLE_FILE_CONTENT "/* */\n")
    set(MACRO_CHECK_SYMBOL_EXISTS_FLAGS ${CMAKE_REQUIRED_FLAGS})
    if(CMAKE_REQUIRED_LINK_OPTIONS)
      set(CHECK_SYMBOL_EXISTS_LINK_OPTIONS
        LINK_OPTIONS ${CMAKE_REQUIRED_LINK_OPTIONS})
    else()
      set(CHECK_SYMBOL_EXISTS_LINK_OPTIONS)
    endif()
    if(CMAKE_REQUIRED_LIBRARIES)
      set(CHECK_SYMBOL_EXISTS_LIBS
        LINK_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
    else()
      set(CHECK_SYMBOL_EXISTS_LIBS)
    endif()
    if(CMAKE_REQUIRED_INCLUDES)
      set(CMAKE_SYMBOL_EXISTS_INCLUDES
        "-DINCLUDE_DIRECTORIES:STRING=${CMAKE_REQUIRED_INCLUDES}")
    else()
      set(CMAKE_SYMBOL_EXISTS_INCLUDES)
    endif()
    foreach(FILE ${FILES})
      string(APPEND CMAKE_CONFIGURABLE_FILE_CONTENT
        "#include <${FILE}>\n")
    endforeach()
    string(APPEND CMAKE_CONFIGURABLE_FILE_CONTENT "
int main(int argc, char** argv)
{
  (void)argv;")
    set(_CSE_CHECK_NON_MACRO "return ((int*)(&${SYMBOL}))[argc];")
    if("${SYMBOL}" MATCHES "^[a-zA-Z_][a-zA-Z0-9_]*$")
      # The SYMBOL has a legal macro name.  Test whether it exists as a macro.
      string(APPEND CMAKE_CONFIGURABLE_FILE_CONTENT "
#ifndef ${SYMBOL}
  ${_CSE_CHECK_NON_MACRO}
#else
  (void)argc;
  return 0;
#endif")
    else()
      # The SYMBOL cannot be a macro (e.g., a template function).
      string(APPEND CMAKE_CONFIGURABLE_FILE_CONTENT "
  ${_CSE_CHECK_NON_MACRO}")
    endif()
    string(APPEND CMAKE_CONFIGURABLE_FILE_CONTENT "
}")
    unset(_CSE_CHECK_NON_MACRO)

    configure_file("${CMAKE_ROOT}/Modules/CMakeConfigurableFile.in"
      "${SOURCEFILE}" @ONLY)

    if(NOT CMAKE_REQUIRED_QUIET)
      message(CHECK_START "Looking for ${SYMBOL}")
    endif()
    try_compile(${VARIABLE}
      ${CMAKE_BINARY_DIR}
      "${SOURCEFILE}"
      COMPILE_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS}
      ${CHECK_SYMBOL_EXISTS_LINK_OPTIONS}
      ${CHECK_SYMBOL_EXISTS_LIBS}
      CMAKE_FLAGS
      -DCOMPILE_DEFINITIONS:STRING=${MACRO_CHECK_SYMBOL_EXISTS_FLAGS}
      "${CMAKE_SYMBOL_EXISTS_INCLUDES}"
      OUTPUT_VARIABLE OUTPUT)
    if(${VARIABLE})
      if(NOT CMAKE_REQUIRED_QUIET)
        message(CHECK_PASS "found")
      endif()
      set(${VARIABLE} 1 CACHE INTERNAL "Have symbol ${SYMBOL}")
      file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
        "Determining if the ${SYMBOL} "
        "exist passed with the following output:\n"
        "${OUTPUT}\nFile ${SOURCEFILE}:\n"
        "${CMAKE_CONFIGURABLE_FILE_CONTENT}\n")
    else()
      if(NOT CMAKE_REQUIRED_QUIET)
        message(CHECK_FAIL "not found")
      endif()
      set(${VARIABLE} "" CACHE INTERNAL "Have symbol ${SYMBOL}")
      file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
        "Determining if the ${SYMBOL} "
        "exist failed with the following output:\n"
        "${OUTPUT}\nFile ${SOURCEFILE}:\n"
        "${CMAKE_CONFIGURABLE_FILE_CONTENT}\n")
    endif()
    unset(CMAKE_CONFIGURABLE_FILE_CONTENT)
  endif()
endmacro()

cmake_policy(POP)
