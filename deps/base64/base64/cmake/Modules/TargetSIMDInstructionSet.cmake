# Written in 2016-2017 by Henrik Steffen Gaßmann henrik@gassmann.onl
#
# To the extent possible under law, the author(s) have dedicated all
# copyright and related and neighboring rights to this software to the
# public domain worldwide. This software is distributed without any warranty.
#
# You should have received a copy of the CC0 Public Domain Dedication
# along with this software. If not, see
#
#     http://creativecommons.org/publicdomain/zero/1.0/
#
########################################################################

########################################################################
# compiler flags definition
macro(define_SIMD_compile_flags)
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
        # x86
        set(COMPILE_FLAGS_SSSE3 "-mssse3")
        set(COMPILE_FLAGS_SSE41 "-msse4.1")
        set(COMPILE_FLAGS_SSE42 "-msse4.2")
        set(COMPILE_FLAGS_AVX "-mavx")
        set(COMPILE_FLAGS_AVX2 "-mavx2")
        set(COMPILE_FLAGS_AVX512 "-mavx512vl -mavx512vbmi")

        #arm
        set(COMPILE_FLAGS_NEON32 "-mfpu=neon")
    elseif(MSVC)
        set(COMPILE_FLAGS_SSSE3 " ")
        set(COMPILE_FLAGS_SSE41 " ")
        set(COMPILE_FLAGS_SSE42 " ")
        set(COMPILE_FLAGS_AVX "/arch:AVX")
        set(COMPILE_FLAGS_AVX2 "/arch:AVX2")
        set(COMPILE_FLAGS_AVX512 "/arch:AVX512")
    endif()
endmacro(define_SIMD_compile_flags)
