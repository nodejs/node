# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
SelectLibraryConfigurations
---------------------------

.. code-block:: cmake

  select_library_configurations(basename)

This macro takes a library base name as an argument, and will choose
good values for the variables

::

  basename_LIBRARY
  basename_LIBRARIES
  basename_LIBRARY_DEBUG
  basename_LIBRARY_RELEASE

depending on what has been found and set.

If only ``basename_LIBRARY_RELEASE`` is defined, ``basename_LIBRARY`` will
be set to the release value, and ``basename_LIBRARY_DEBUG`` will be set
to ``basename_LIBRARY_DEBUG-NOTFOUND``.  If only ``basename_LIBRARY_DEBUG``
is defined, then ``basename_LIBRARY`` will take the debug value, and
``basename_LIBRARY_RELEASE`` will be set to ``basename_LIBRARY_RELEASE-NOTFOUND``.

If the generator supports configuration types, then ``basename_LIBRARY``
and ``basename_LIBRARIES`` will be set with debug and optimized flags
specifying the library to be used for the given configuration.  If no
build type has been set or the generator in use does not support
configuration types, then ``basename_LIBRARY`` and ``basename_LIBRARIES``
will take only the release value, or the debug value if the release one
is not set.
#]=======================================================================]

# This macro was adapted from the FindQt4 CMake module and is maintained by Will
# Dicharry <wdicharry@stellarscience.com>.

macro(select_library_configurations basename)
    if(NOT ${basename}_LIBRARY_RELEASE)
        set(${basename}_LIBRARY_RELEASE "${basename}_LIBRARY_RELEASE-NOTFOUND" CACHE FILEPATH "Path to a library.")
    endif()
    if(NOT ${basename}_LIBRARY_DEBUG)
        set(${basename}_LIBRARY_DEBUG "${basename}_LIBRARY_DEBUG-NOTFOUND" CACHE FILEPATH "Path to a library.")
    endif()

    get_property(_isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if( ${basename}_LIBRARY_DEBUG AND ${basename}_LIBRARY_RELEASE AND
           NOT ${basename}_LIBRARY_DEBUG STREQUAL ${basename}_LIBRARY_RELEASE AND
           ( _isMultiConfig OR CMAKE_BUILD_TYPE ) )
        # if the generator is multi-config or if CMAKE_BUILD_TYPE is set for
        # single-config generators, set optimized and debug libraries
        set( ${basename}_LIBRARY "" )
        foreach( _libname IN LISTS ${basename}_LIBRARY_RELEASE )
            list( APPEND ${basename}_LIBRARY optimized "${_libname}" )
        endforeach()
        foreach( _libname IN LISTS ${basename}_LIBRARY_DEBUG )
            list( APPEND ${basename}_LIBRARY debug "${_libname}" )
        endforeach()
    elseif( ${basename}_LIBRARY_RELEASE )
        set( ${basename}_LIBRARY ${${basename}_LIBRARY_RELEASE} )
    elseif( ${basename}_LIBRARY_DEBUG )
        set( ${basename}_LIBRARY ${${basename}_LIBRARY_DEBUG} )
    else()
        set( ${basename}_LIBRARY "${basename}_LIBRARY-NOTFOUND")
    endif()

    set( ${basename}_LIBRARIES "${${basename}_LIBRARY}" )

    if( ${basename}_LIBRARY )
        set( ${basename}_FOUND TRUE )
    endif()

    mark_as_advanced( ${basename}_LIBRARY_RELEASE
        ${basename}_LIBRARY_DEBUG
    )
endmacro()
