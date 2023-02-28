# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
CheckLibraryExists
------------------

Check if the function exists.

.. command:: CHECK_LIBRARY_EXISTS

  .. code-block:: cmake

    CHECK_LIBRARY_EXISTS(LIBRARY FUNCTION LOCATION VARIABLE)

  ::

    LIBRARY  - the name of the library you are looking for
    FUNCTION - the name of the function
    LOCATION - location where the library should be found
    VARIABLE - variable to store the result
               Will be created as an internal cache variable.



The following variables may be set before calling this macro to modify
the way the check is run:

::

  CMAKE_REQUIRED_FLAGS = string of compile command line flags
  CMAKE_REQUIRED_DEFINITIONS = list of macros to define (-DFOO=bar)
  CMAKE_REQUIRED_LINK_OPTIONS = list of options to pass to link command
  CMAKE_REQUIRED_LIBRARIES = list of libraries to link
  CMAKE_REQUIRED_QUIET = execute quietly without messages
#]=======================================================================]

if(__CheckLibraryExists_cmake__)
  return()
endif()
set(__CheckLibraryExists_cmake__ TRUE)

macro(CHECK_LIBRARY_EXISTS LIBRARY FUNCTION LOCATION VARIABLE)
  if(NOT DEFINED "${VARIABLE}")
    set(MACRO_CHECK_LIBRARY_EXISTS_DEFINITION
      "-DCHECK_FUNCTION_EXISTS=${FUNCTION} ${CMAKE_REQUIRED_FLAGS}")
    if(NOT CMAKE_REQUIRED_QUIET)
      message(CHECK_START "Looking for ${FUNCTION} in ${LIBRARY}")
    endif()
    set(CHECK_LIBRARY_EXISTS_LINK_OPTIONS)
    if(CMAKE_REQUIRED_LINK_OPTIONS)
      set(CHECK_LIBRARY_EXISTS_LINK_OPTIONS
        LINK_OPTIONS ${CMAKE_REQUIRED_LINK_OPTIONS})
    endif()
    set(CHECK_LIBRARY_EXISTS_LIBRARIES ${LIBRARY})
    if(CMAKE_REQUIRED_LIBRARIES)
      set(CHECK_LIBRARY_EXISTS_LIBRARIES
        ${CHECK_LIBRARY_EXISTS_LIBRARIES} ${CMAKE_REQUIRED_LIBRARIES})
    endif()

    if(CMAKE_C_COMPILER_LOADED)
      set(_cle_source ${CMAKE_ROOT}/Modules/CheckFunctionExists.c)
    elseif(CMAKE_CXX_COMPILER_LOADED)
      set(_cle_source ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CheckLibraryExists/CheckFunctionExists.cxx)
      configure_file(${CMAKE_ROOT}/Modules/CheckFunctionExists.c "${_cle_source}" COPYONLY)
    else()
      message(FATAL_ERROR "CHECK_FUNCTION_EXISTS needs either C or CXX language enabled")
    endif()

    try_compile(${VARIABLE}
      ${CMAKE_BINARY_DIR}
      ${_cle_source}
      COMPILE_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS}
      ${CHECK_LIBRARY_EXISTS_LINK_OPTIONS}
      LINK_LIBRARIES ${CHECK_LIBRARY_EXISTS_LIBRARIES}
      CMAKE_FLAGS
      -DCOMPILE_DEFINITIONS:STRING=${MACRO_CHECK_LIBRARY_EXISTS_DEFINITION}
      -DLINK_DIRECTORIES:STRING=${LOCATION}
      OUTPUT_VARIABLE OUTPUT)
    unset(_cle_source)

    if(${VARIABLE})
      if(NOT CMAKE_REQUIRED_QUIET)
        message(CHECK_PASS "found")
      endif()
      set(${VARIABLE} 1 CACHE INTERNAL "Have library ${LIBRARY}")
      file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
        "Determining if the function ${FUNCTION} exists in the ${LIBRARY} "
        "passed with the following output:\n"
        "${OUTPUT}\n\n")
    else()
      if(NOT CMAKE_REQUIRED_QUIET)
        message(CHECK_FAIL "not found")
      endif()
      set(${VARIABLE} "" CACHE INTERNAL "Have library ${LIBRARY}")
      file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
        "Determining if the function ${FUNCTION} exists in the ${LIBRARY} "
        "failed with the following output:\n"
        "${OUTPUT}\n\n")
    endif()
  endif()
endmacro()
