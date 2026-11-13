add_subdirectory(memory_utils)

add_header_library(
  string_utils
  HDRS
    string_utils.h
  DEPENDS
    libc.hdr.types.size_t
    libc.hdr.limits_macros
    libc.hdr.stdint_proxy
    libc.src.__support.CPP.bitset
    libc.src.__support.CPP.type_traits
    libc.src.__support.CPP.simd
    libc.src.__support.common
    libc.src.__support.macros.attributes
    libc.src.string.memory_utils.inline_memcpy
)

add_header_library(
  allocating_string_utils
  HDRS
    allocating_string_utils.h
  DEPENDS
    .memory_utils.inline_memcpy
    libc.include.stdlib
    libc.src.__support.CPP.optional
    libc.src.__support.macros.config
)

add_entrypoint_object(
  memccpy
  SRCS
    memccpy.cpp
  HDRS
    memccpy.h
)

add_entrypoint_object(
  mempcpy
  SRCS
    mempcpy.cpp
  HDRS
    mempcpy.h
  DEPENDS
    .memory_utils.inline_memcpy
)

add_entrypoint_object(
  memmem
  SRCS
    memmem.cpp
  HDRS
    memmem.h
  DEPENDS
    .memory_utils.inline_memmem
)

add_entrypoint_object(
  memchr
  SRCS
    memchr.cpp
  HDRS
    memchr.h
  DEPENDS
    .string_utils
)

add_entrypoint_object(
  memrchr
  SRCS
    memrchr.cpp
  HDRS
    memrchr.h
)

add_entrypoint_object(
  stpcpy
  SRCS
    stpcpy.cpp
  HDRS
    stpcpy.h
  DEPENDS
    .string_utils
)

add_entrypoint_object(
  stpncpy
  SRCS
    stpncpy.cpp
  HDRS
    stpncpy.h
  DEPENDS
    .memory_utils.inline_bzero
)

add_entrypoint_object(
  strcat
  SRCS
    strcat.cpp
  HDRS
    strcat.h
  DEPENDS
    .string_utils
    libc.include.llvm-libc-types.size_t
)

add_entrypoint_object(
  strchr
  SRCS
    strchr.cpp
  HDRS
    strchr.h
  DEPENDS
    .string_utils
)

add_entrypoint_object(
  strchrnul
  SRCS
    strchrnul.cpp
  HDRS
    strchrnul.h
  DEPENDS
    .string_utils
)

add_entrypoint_object(
  strcmp
  SRCS
    strcmp.cpp
  HDRS
    strcmp.h
  DEPENDS
    .memory_utils.inline_strcmp
)

add_entrypoint_object(
  strcasestr
  SRCS
    strcasestr.cpp
  HDRS
    strcasestr.h
  DEPENDS
    .memory_utils.inline_strstr
    libc.src.__support.ctype_utils
)

add_entrypoint_object(
  strcoll
  SRCS
    strcoll.cpp
  HDRS
    strcoll.h
  DEPENDS
    .string_utils
    libc.src.__support.common
    libc.src.__support.macros.config
    libc.src.__support.macros.null_check
)

add_entrypoint_object(
  strcoll_l
  SRCS
    strcoll_l.cpp
  HDRS
    strcoll_l.h
  DEPENDS
    .string_utils
    libc.src.__support.common
    libc.src.__support.macros.config
    libc.src.__support.macros.null_check
)

add_entrypoint_object(
  strcpy
  SRCS
    strcpy.cpp
  HDRS
    strcpy.h
  DEPENDS
    .memory_utils.inline_memcpy
    .string_utils
)

add_entrypoint_object(
  strcspn
  SRCS
    strcspn.cpp
  HDRS
    strcspn.h
  DEPENDS
    .string_utils
)

add_entrypoint_object(
  strdup
  SRCS
    strdup.cpp
  HDRS
    strdup.h
  DEPENDS
    .memory_utils.inline_memcpy
    .string_utils
    libc.hdr.stdlib_macros
    libc.src.errno.errno
    libc.include.llvm-libc-types.size_t
)

add_entrypoint_object(
  strerror
  SRCS
    strerror.cpp
  HDRS
    strerror.h
  DEPENDS
    libc.src.__support.StringUtil.error_to_string
)

add_entrypoint_object(
  strerror_r
  SRCS
    strerror_r.cpp
  HDRS
    strerror_r.h
  DEPENDS
    libc.src.__support.StringUtil.error_to_string
)

add_entrypoint_object(
  strlcat
  SRCS
    strlcat.cpp
  HDRS
    strlcat.h
  DEPENDS
    .string_utils
    libc.include.llvm-libc-types.size_t
)

add_entrypoint_object(
  strlcpy
  SRCS
    strlcpy.cpp
  HDRS
    strlcpy.h
  DEPENDS
    .string_utils
    libc.include.llvm-libc-types.size_t
)

add_entrypoint_object(
  strlen
  SRCS
    strlen.cpp
  HDRS
    strlen.h
  DEPENDS
    .string_utils
    libc.include.llvm-libc-types.size_t
)

add_entrypoint_object(
  strncat
  SRCS
    strncat.cpp
  HDRS
    strncat.h
  DEPENDS
    .string_utils
    libc.include.llvm-libc-types.size_t
)

add_entrypoint_object(
  strncmp
  SRCS
    strncmp.cpp
  HDRS
    strncmp.h
  DEPENDS
    .memory_utils.inline_strcmp
)

add_entrypoint_object(
  strncpy
  SRCS
    strncpy.cpp
  HDRS
    strncpy.h
)

add_entrypoint_object(
  strndup
  SRCS
    strndup.cpp
  HDRS
    strndup.h
  DEPENDS
    .memory_utils.inline_memcpy
    .string_utils
    libc.include.stdlib
    libc.src.__support.CPP.new
    libc.include.llvm-libc-types.size_t
)

add_entrypoint_object(
  strnlen
  SRCS
    strnlen.cpp
  HDRS
    strnlen.h
  DEPENDS
    .string_utils
)

add_entrypoint_object(
  strnlen_s
  SRCS
    strnlen_s.cpp
  HDRS
    strnlen_s.h
  DEPENDS
    libc.hdr.types.size_t
    .string_utils
    libc.src.__support.common
    libc.src.__support.macros.config
)

add_entrypoint_object(
  strpbrk
  SRCS
    strpbrk.cpp
  HDRS
    strpbrk.h
  DEPENDS
    .string_utils
)

add_entrypoint_object(
  strrchr
  SRCS
    strrchr.cpp
  HDRS
    strrchr.h
  DEPENDS
    .string_utils
)

add_entrypoint_object(
  strsep
  SRCS
    strsep.cpp
  HDRS
    strsep.h
  DEPENDS
    .string_utils
)

add_entrypoint_object(
  strsignal
  SRCS
    strsignal.cpp
  HDRS
    strsignal.h
  DEPENDS
    libc.src.__support.StringUtil.signal_to_string
)

add_entrypoint_object(
  strspn
  SRCS
    strspn.cpp
  HDRS
    strspn.h
  DEPENDS
    libc.src.__support.CPP.bitset
)

add_entrypoint_object(
  strstr
  SRCS
    strstr.cpp
  HDRS
    strstr.h
  DEPENDS
    .memory_utils.inline_strstr
)

add_entrypoint_object(
  strtok
  SRCS
    strtok.cpp
  HDRS
    strtok.h
  DEPENDS
    .string_utils
)

add_entrypoint_object(
  strtok_r
  SRCS
    strtok_r.cpp
  HDRS
    strtok_r.h
  DEPENDS
    .string_utils
)

add_entrypoint_object(
  strxfrm
  SRCS
    strxfrm.cpp
  HDRS
    strxfrm.h
  DEPENDS
    .string_utils
    .memory_utils.inline_memcpy
)

add_entrypoint_object(
  strxfrm_l
  SRCS
    strxfrm_l.cpp
  HDRS
    strxfrm_l.h
  DEPENDS
    .string_utils
    .memory_utils.inline_memcpy
)

add_entrypoint_object(
  memset_explicit
  SRCS
    memset_explicit.cpp
  HDRS
    memset_explicit.h
  DEPENDS
    .string_utils
    .memory_utils.inline_memset
)

# ------------------------------------------------------------------------------
# memcmp
# ------------------------------------------------------------------------------

function(add_memcmp memcmp_name)
  add_implementation(memcmp ${memcmp_name}
    SRCS ${LIBC_SOURCE_DIR}/src/string/memcmp.cpp
    HDRS ${LIBC_SOURCE_DIR}/src/string/memcmp.h
    DEPENDS
      .memory_utils.inline_memcmp
      libc.include.string
    ${ARGN}
  )
endfunction()

if(${LIBC_TARGET_ARCHITECTURE_IS_X86_64})
  add_memcmp(memcmp_x86_64_opt_sse2 COMPILE_OPTIONS -march=k8 REQUIRE SSE2)
  add_memcmp(memcmp_x86_64_opt_sse4 COMPILE_OPTIONS -march=nehalem REQUIRE SSE4_2)
  add_memcmp(memcmp_x86_64_opt_avx2 COMPILE_OPTIONS -march=haswell REQUIRE AVX2)
  add_memcmp(memcmp_x86_64_opt_avx512 COMPILE_OPTIONS -march=skylake-avx512 REQUIRE AVX512BW)
  add_memcmp(memcmp_opt_host COMPILE_OPTIONS ${LIBC_COMPILE_OPTIONS_NATIVE})
  add_memcmp(memcmp)
elseif(${LIBC_TARGET_ARCHITECTURE_IS_AARCH64})
  add_memcmp(memcmp_opt_host COMPILE_OPTIONS ${LIBC_COMPILE_OPTIONS_NATIVE})
  add_memcmp(memcmp)
elseif(LIBC_TARGET_OS_IS_GPU)
  add_memcmp(memcmp)
else()
  add_memcmp(memcmp_opt_host COMPILE_OPTIONS ${LIBC_COMPILE_OPTIONS_NATIVE})
  add_memcmp(memcmp)
endif()

# ------------------------------------------------------------------------------
# memcpy
# ------------------------------------------------------------------------------

function(add_memcpy memcpy_name)
  add_implementation(memcpy ${memcpy_name}
    SRCS ${LIBC_SOURCE_DIR}/src/string/memcpy.cpp
    HDRS ${LIBC_SOURCE_DIR}/src/string/memcpy.h
    DEPENDS
      .memory_utils.inline_memcpy
      libc.include.string
    ${ARGN}
  )
endfunction()

if(${LIBC_TARGET_ARCHITECTURE_IS_X86_64})
  add_memcpy(memcpy_x86_64_opt_sse2 COMPILE_OPTIONS -march=k8 REQUIRE SSE2)
  add_memcpy(memcpy_x86_64_opt_sse4 COMPILE_OPTIONS -march=nehalem REQUIRE SSE4_2)
  add_memcpy(memcpy_x86_64_opt_avx COMPILE_OPTIONS -march=sandybridge REQUIRE AVX)
  add_memcpy(memcpy_x86_64_opt_avx512 COMPILE_OPTIONS -march=skylake-avx512 REQUIRE AVX512F)
  add_memcpy(memcpy_x86_64_opt_sw_prefetch_sse4 COMPILE_OPTIONS -DLIBC_COPT_MEMCPY_X86_USE_SOFTWARE_PREFETCHING -march=nehalem REQUIRE SSE4_2)
  add_memcpy(memcpy_x86_64_opt_sw_prefetch_avx COMPILE_OPTIONS -DLIBC_COPT_MEMCPY_X86_USE_SOFTWARE_PREFETCHING -march=sandybridge REQUIRE AVX)
  add_memcpy(memcpy_opt_host COMPILE_OPTIONS ${LIBC_COMPILE_OPTIONS_NATIVE})
  add_memcpy(memcpy)
elseif(${LIBC_TARGET_ARCHITECTURE_IS_AARCH64})
  # Disable tail merging as it leads to lower performance.
  add_memcpy(memcpy_opt_host COMPILE_OPTIONS ${LIBC_COMPILE_OPTIONS_NATIVE}
    MLLVM_COMPILE_OPTIONS "-tail-merge-threshold=0")
  add_memcpy(memcpy MLLVM_COMPILE_OPTIONS "-tail-merge-threshold=0")
elseif(LIBC_TARGET_OS_IS_GPU)
  add_memcpy(memcpy)
else()
  add_memcpy(memcpy_opt_host COMPILE_OPTIONS ${LIBC_COMPILE_OPTIONS_NATIVE})
  add_memcpy(memcpy)
endif()

# ------------------------------------------------------------------------------
# memmove
# ------------------------------------------------------------------------------

function(add_memmove memmove_name)
  add_implementation(memmove ${memmove_name}
    SRCS ${LIBC_SOURCE_DIR}/src/string/memmove.cpp
    HDRS ${LIBC_SOURCE_DIR}/src/string/memmove.h
    DEPENDS
      .memory_utils.inline_memcpy
      libc.include.string
    ${ARGN}
  )
endfunction()

if(${LIBC_TARGET_ARCHITECTURE_IS_X86_64})
  add_memmove(memmove_x86_64_opt_sse2 COMPILE_OPTIONS -march=k8 REQUIRE SSE2)
  add_memmove(memmove_x86_64_opt_sse4 COMPILE_OPTIONS -march=nehalem REQUIRE SSE4_2)
  add_memmove(memmove_x86_64_opt_avx2 COMPILE_OPTIONS -march=haswell REQUIRE AVX2)
  add_memmove(memmove_x86_64_opt_avx512 COMPILE_OPTIONS -march=skylake-avx512 REQUIRE AVX512F)
  add_memmove(memmove_opt_host COMPILE_OPTIONS ${LIBC_COMPILE_OPTIONS_NATIVE})
  add_memmove(memmove)
elseif(${LIBC_TARGET_ARCHITECTURE_IS_AARCH64})
  # Disable tail merging as it leads to lower performance.
  add_memmove(memmove_opt_host COMPILE_OPTIONS ${LIBC_COMPILE_OPTIONS_NATIVE}
    MLLVM_COMPILE_OPTIONS "-tail-merge-threshold=0")
  add_memmove(memmove MLLVM_COMPILE_OPTIONS "-tail-merge-threshold=0")
elseif(LIBC_TARGET_OS_IS_GPU)
  add_memmove(memmove)
else()
  add_memmove(memmove_opt_host COMPILE_OPTIONS ${LIBC_COMPILE_OPTIONS_NATIVE})
  add_memmove(memmove)
endif()

# ------------------------------------------------------------------------------
# memset
# ------------------------------------------------------------------------------

function(add_memset memset_name)
  add_implementation(memset ${memset_name}
    SRCS ${LIBC_SOURCE_DIR}/src/string/memset.cpp
    HDRS ${LIBC_SOURCE_DIR}/src/string/memset.h
    DEPENDS
      .memory_utils.inline_memset
      libc.include.string
    ${ARGN}
  )
endfunction()

if(${LIBC_TARGET_ARCHITECTURE_IS_X86_64})
  add_memset(memset_x86_64_opt_sse2 COMPILE_OPTIONS -march=k8 REQUIRE SSE2)
  add_memset(memset_x86_64_opt_sse4 COMPILE_OPTIONS -march=nehalem REQUIRE SSE4_2)
  add_memset(memset_x86_64_opt_avx2 COMPILE_OPTIONS -march=haswell REQUIRE AVX2)
  add_memset(memset_x86_64_opt_avx512 COMPILE_OPTIONS -march=skylake-avx512 REQUIRE AVX512F)
  add_memset(memset_x86_64_opt_sw_prefetch COMPILE_OPTIONS -DLIBC_COPT_MEMSET_X86_USE_SOFTWARE_PREFETCHING)
  add_memset(memset_opt_host COMPILE_OPTIONS ${LIBC_COMPILE_OPTIONS_NATIVE})
  add_memset(memset)
elseif(${LIBC_TARGET_ARCHITECTURE_IS_AARCH64})
  # Disable tail merging as it leads to lower performance.
  add_memset(memset_opt_host COMPILE_OPTIONS ${LIBC_COMPILE_OPTIONS_NATIVE}
    MLLVM_COMPILE_OPTIONS "-tail-merge-threshold=0")
  add_memset(memset MLLVM_COMPILE_OPTIONS "-tail-merge-threshold=0")
elseif(LIBC_TARGET_OS_IS_GPU)
  add_memset(memset)
else()
  add_memset(memset_opt_host COMPILE_OPTIONS ${LIBC_COMPILE_OPTIONS_NATIVE})
  add_memset(memset)
endif()
