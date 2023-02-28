# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
CMakePushCheckState
-------------------



This module defines three macros: ``CMAKE_PUSH_CHECK_STATE()``
``CMAKE_POP_CHECK_STATE()`` and ``CMAKE_RESET_CHECK_STATE()`` These macros can
be used to save, restore and reset (i.e., clear contents) the state of
the variables ``CMAKE_REQUIRED_FLAGS``, ``CMAKE_REQUIRED_DEFINITIONS``,
``CMAKE_REQUIRED_LINK_OPTIONS``, ``CMAKE_REQUIRED_LIBRARIES``,
``CMAKE_REQUIRED_INCLUDES`` and ``CMAKE_EXTRA_INCLUDE_FILES`` used by the
various Check-files coming with CMake, like e.g. ``check_function_exists()``
etc.
The variable contents are pushed on a stack, pushing multiple times is
supported.  This is useful e.g.  when executing such tests in a Find-module,
where they have to be set, but after the Find-module has been executed they
should have the same value as they had before.

``CMAKE_PUSH_CHECK_STATE()`` macro receives optional argument ``RESET``.
Whether it's specified, ``CMAKE_PUSH_CHECK_STATE()`` will set all
``CMAKE_REQUIRED_*`` variables to empty values, same as
``CMAKE_RESET_CHECK_STATE()`` call will do.

Usage:

.. code-block:: cmake

   cmake_push_check_state(RESET)
   set(CMAKE_REQUIRED_DEFINITIONS -DSOME_MORE_DEF)
   check_function_exists(...)
   cmake_reset_check_state()
   set(CMAKE_REQUIRED_DEFINITIONS -DANOTHER_DEF)
   check_function_exists(...)
   cmake_pop_check_state()
#]=======================================================================]

macro(CMAKE_RESET_CHECK_STATE)

  set(CMAKE_EXTRA_INCLUDE_FILES)
  set(CMAKE_REQUIRED_INCLUDES)
  set(CMAKE_REQUIRED_DEFINITIONS)
  set(CMAKE_REQUIRED_LINK_OPTIONS)
  set(CMAKE_REQUIRED_LIBRARIES)
  set(CMAKE_REQUIRED_FLAGS)
  set(CMAKE_REQUIRED_QUIET)

endmacro()

macro(CMAKE_PUSH_CHECK_STATE)

  if(NOT DEFINED _CMAKE_PUSH_CHECK_STATE_COUNTER)
    set(_CMAKE_PUSH_CHECK_STATE_COUNTER 0)
  endif()

  math(EXPR _CMAKE_PUSH_CHECK_STATE_COUNTER "${_CMAKE_PUSH_CHECK_STATE_COUNTER}+1")

  set(_CMAKE_EXTRA_INCLUDE_FILES_SAVE_${_CMAKE_PUSH_CHECK_STATE_COUNTER}    ${CMAKE_EXTRA_INCLUDE_FILES})
  set(_CMAKE_REQUIRED_INCLUDES_SAVE_${_CMAKE_PUSH_CHECK_STATE_COUNTER}      ${CMAKE_REQUIRED_INCLUDES})
  set(_CMAKE_REQUIRED_DEFINITIONS_SAVE_${_CMAKE_PUSH_CHECK_STATE_COUNTER}   ${CMAKE_REQUIRED_DEFINITIONS})
  set(_CMAKE_REQUIRED_LINK_OPTIONS_SAVE_${_CMAKE_PUSH_CHECK_STATE_COUNTER}  ${CMAKE_REQUIRED_LINK_OPTIONS})
  set(_CMAKE_REQUIRED_LIBRARIES_SAVE_${_CMAKE_PUSH_CHECK_STATE_COUNTER}     ${CMAKE_REQUIRED_LIBRARIES})
  set(_CMAKE_REQUIRED_FLAGS_SAVE_${_CMAKE_PUSH_CHECK_STATE_COUNTER}         ${CMAKE_REQUIRED_FLAGS})
  set(_CMAKE_REQUIRED_QUIET_SAVE_${_CMAKE_PUSH_CHECK_STATE_COUNTER}         ${CMAKE_REQUIRED_QUIET})

  if (${ARGC} GREATER 0 AND "${ARGV0}" STREQUAL "RESET")
    cmake_reset_check_state()
  endif()

endmacro()

macro(CMAKE_POP_CHECK_STATE)

# don't pop more than we pushed
  if("${_CMAKE_PUSH_CHECK_STATE_COUNTER}" GREATER "0")

    set(CMAKE_EXTRA_INCLUDE_FILES    ${_CMAKE_EXTRA_INCLUDE_FILES_SAVE_${_CMAKE_PUSH_CHECK_STATE_COUNTER}})
    set(CMAKE_REQUIRED_INCLUDES      ${_CMAKE_REQUIRED_INCLUDES_SAVE_${_CMAKE_PUSH_CHECK_STATE_COUNTER}})
    set(CMAKE_REQUIRED_DEFINITIONS   ${_CMAKE_REQUIRED_DEFINITIONS_SAVE_${_CMAKE_PUSH_CHECK_STATE_COUNTER}})
    set(CMAKE_REQUIRED_LINK_OPTIONS  ${_CMAKE_REQUIRED_LINK_OPTIONS_SAVE_${_CMAKE_PUSH_CHECK_STATE_COUNTER}})
    set(CMAKE_REQUIRED_LIBRARIES     ${_CMAKE_REQUIRED_LIBRARIES_SAVE_${_CMAKE_PUSH_CHECK_STATE_COUNTER}})
    set(CMAKE_REQUIRED_FLAGS         ${_CMAKE_REQUIRED_FLAGS_SAVE_${_CMAKE_PUSH_CHECK_STATE_COUNTER}})
    set(CMAKE_REQUIRED_QUIET         ${_CMAKE_REQUIRED_QUIET_SAVE_${_CMAKE_PUSH_CHECK_STATE_COUNTER}})

    math(EXPR _CMAKE_PUSH_CHECK_STATE_COUNTER "${_CMAKE_PUSH_CHECK_STATE_COUNTER}-1")
  endif()

endmacro()
