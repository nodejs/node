// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_BUILD_CONFIG_H_
#define V8_BASE_BUILD_CONFIG_H_

#include "include/v8config.h"

// Processor architecture detection.  For more info on what's defined, see:
//   http://msdn.microsoft.com/en-us/library/b0084kay.aspx
//   http://www.agner.org/optimize/calling_conventions.pdf
//   or with gcc, run: "echo | gcc -E -dM -"
#if defined(_M_X64) || defined(__x86_64__)
#define V8_HOST_ARCH_X64 1
#if defined(__x86_64__) && __SIZEOF_POINTER__ == 4  // Check for x32.
#define V8_HOST_ARCH_32_BIT 1
#else
#define V8_HOST_ARCH_64_BIT 1
#endif
#elif defined(_M_IX86) || defined(__i386__)
#define V8_HOST_ARCH_IA32 1
#define V8_HOST_ARCH_32_BIT 1
#elif defined(__AARCH64EL__) || defined(_M_ARM64)
#define V8_HOST_ARCH_ARM64 1
#define V8_HOST_ARCH_64_BIT 1
#elif defined(__ARMEL__)
#define V8_HOST_ARCH_ARM 1
#define V8_HOST_ARCH_32_BIT 1
#elif defined(__mips64)
#define V8_HOST_ARCH_MIPS64 1
#define V8_HOST_ARCH_64_BIT 1
#elif defined(__MIPSEB__) || defined(__MIPSEL__)
#define V8_HOST_ARCH_MIPS 1
#define V8_HOST_ARCH_32_BIT 1
#elif defined(__loongarch64)
#define V8_HOST_ARCH_LOONG64 1
#define V8_HOST_ARCH_64_BIT 1
#elif defined(__PPC64__) || defined(_ARCH_PPC64)
#define V8_HOST_ARCH_PPC64 1
#define V8_HOST_ARCH_64_BIT 1
#elif defined(__PPC__) || defined(_ARCH_PPC)
#define V8_HOST_ARCH_PPC 1
#define V8_HOST_ARCH_32_BIT 1
#elif defined(__s390__) || defined(__s390x__)
#define V8_HOST_ARCH_S390 1
#if defined(__s390x__)
#define V8_HOST_ARCH_64_BIT 1
#else
#define V8_HOST_ARCH_32_BIT 1
#endif
#elif defined(__riscv) || defined(__riscv__)
#if __riscv_xlen == 64
#define V8_HOST_ARCH_RISCV64 1
#define V8_HOST_ARCH_64_BIT 1
#else
#error "Cannot detect Riscv's bitwidth"
#endif
#else
#error "Host architecture was not detected as supported by v8"
#endif

#if defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || \
    defined(__ARM_ARCH_7__)
#define CAN_USE_ARMV7_INSTRUCTIONS 1
#ifdef __ARM_ARCH_EXT_IDIV__
#define CAN_USE_SUDIV 1
#endif
#ifndef CAN_USE_VFP3_INSTRUCTIONS
#define CAN_USE_VFP3_INSTRUCTIONS 1
#endif
#endif

#if defined(__ARM_ARCH_8A__)
#define CAN_USE_ARMV7_INSTRUCTIONS 1
#define CAN_USE_SUDIV 1
#define CAN_USE_ARMV8_INSTRUCTIONS 1
#ifndef CAN_USE_VFP3_INSTRUCTIONS
#define CAN_USE_VFP3_INSTRUCTIONS 1
#endif
#endif

// Target architecture detection. This may be set externally. If not, detect
// in the same way as the host architecture, that is, target the native
// environment as presented by the compiler.
#if !V8_TARGET_ARCH_X64 && !V8_TARGET_ARCH_IA32 && !V8_TARGET_ARCH_ARM &&      \
    !V8_TARGET_ARCH_ARM64 && !V8_TARGET_ARCH_MIPS && !V8_TARGET_ARCH_MIPS64 && \
    !V8_TARGET_ARCH_PPC && !V8_TARGET_ARCH_PPC64 && !V8_TARGET_ARCH_S390 &&    \
    !V8_TARGET_ARCH_RISCV64 && !V8_TARGET_ARCH_LOONG64
#if defined(_M_X64) || defined(__x86_64__)
#define V8_TARGET_ARCH_X64 1
#elif defined(_M_IX86) || defined(__i386__)
#define V8_TARGET_ARCH_IA32 1
#elif defined(__AARCH64EL__) || defined(_M_ARM64)
#define V8_TARGET_ARCH_ARM64 1
#elif defined(__ARMEL__)
#define V8_TARGET_ARCH_ARM 1
#elif defined(__mips64)
#define V8_TARGET_ARCH_MIPS64 1
#elif defined(__MIPSEB__) || defined(__MIPSEL__)
#define V8_TARGET_ARCH_MIPS 1
#elif defined(_ARCH_PPC64)
#define V8_TARGET_ARCH_PPC64 1
#elif defined(_ARCH_PPC)
#define V8_TARGET_ARCH_PPC 1
#elif defined(__riscv) || defined(__riscv__)
#if __riscv_xlen == 64
#define V8_TARGET_ARCH_RISCV64 1
#endif
#else
#error Target architecture was not detected as supported by v8
#endif
#endif

// Determine architecture pointer size.
#if V8_TARGET_ARCH_IA32
#define V8_TARGET_ARCH_32_BIT 1
#elif V8_TARGET_ARCH_X64
#if !V8_TARGET_ARCH_32_BIT && !V8_TARGET_ARCH_64_BIT
#if defined(__x86_64__) && __SIZEOF_POINTER__ == 4  // Check for x32.
#define V8_TARGET_ARCH_32_BIT 1
#else
#define V8_TARGET_ARCH_64_BIT 1
#endif
#endif
#elif V8_TARGET_ARCH_ARM
#define V8_TARGET_ARCH_32_BIT 1
#elif V8_TARGET_ARCH_ARM64
#define V8_TARGET_ARCH_64_BIT 1
#elif V8_TARGET_ARCH_MIPS
#define V8_TARGET_ARCH_32_BIT 1
#elif V8_TARGET_ARCH_MIPS64
#define V8_TARGET_ARCH_64_BIT 1
#elif V8_TARGET_ARCH_LOONG64
#define V8_TARGET_ARCH_64_BIT 1
#elif V8_TARGET_ARCH_PPC
#define V8_TARGET_ARCH_32_BIT 1
#elif V8_TARGET_ARCH_PPC64
#define V8_TARGET_ARCH_64_BIT 1
#elif V8_TARGET_ARCH_S390
#if V8_TARGET_ARCH_S390X
#define V8_TARGET_ARCH_64_BIT 1
#else
#define V8_TARGET_ARCH_32_BIT 1
#endif
#elif V8_TARGET_ARCH_RISCV64
#define V8_TARGET_ARCH_64_BIT 1
#else
#error Unknown target architecture pointer size
#endif

// Check for supported combinations of host and target architectures.
#if V8_TARGET_ARCH_IA32 && !V8_HOST_ARCH_IA32
#error Target architecture ia32 is only supported on ia32 host
#endif
#if (V8_TARGET_ARCH_X64 && V8_TARGET_ARCH_64_BIT && \
     !((V8_HOST_ARCH_X64 || V8_HOST_ARCH_ARM64) && V8_HOST_ARCH_64_BIT))
#error Target architecture x64 is only supported on x64 and arm64 host
#endif
#if (V8_TARGET_ARCH_X64 && V8_TARGET_ARCH_32_BIT && \
     !(V8_HOST_ARCH_X64 && V8_HOST_ARCH_32_BIT))
#error Target architecture x32 is only supported on x64 host with x32 support
#endif
#if (V8_TARGET_ARCH_ARM && !(V8_HOST_ARCH_IA32 || V8_HOST_ARCH_ARM))
#error Target architecture arm is only supported on arm and ia32 host
#endif
#if (V8_TARGET_ARCH_ARM64 && !(V8_HOST_ARCH_X64 || V8_HOST_ARCH_ARM64))
#error Target architecture arm64 is only supported on arm64 and x64 host
#endif
#if (V8_TARGET_ARCH_MIPS && !(V8_HOST_ARCH_IA32 || V8_HOST_ARCH_MIPS))
#error Target architecture mips is only supported on mips and ia32 host
#endif
#if (V8_TARGET_ARCH_MIPS64 && !(V8_HOST_ARCH_X64 || V8_HOST_ARCH_MIPS64))
#error Target architecture mips64 is only supported on mips64 and x64 host
#endif
#if (V8_TARGET_ARCH_RISCV64 && !(V8_HOST_ARCH_X64 || V8_HOST_ARCH_RISCV64))
#error Target architecture riscv64 is only supported on riscv64 and x64 host
#endif
#if (V8_TARGET_ARCH_LOONG64 && !(V8_HOST_ARCH_X64 || V8_HOST_ARCH_LOONG64))
#error Target architecture loong64 is only supported on loong64 and x64 host
#endif

// Determine architecture endianness.
#if V8_TARGET_ARCH_IA32
#define V8_TARGET_LITTLE_ENDIAN 1
#elif V8_TARGET_ARCH_X64
#define V8_TARGET_LITTLE_ENDIAN 1
#elif V8_TARGET_ARCH_ARM
#define V8_TARGET_LITTLE_ENDIAN 1
#elif V8_TARGET_ARCH_ARM64
#define V8_TARGET_LITTLE_ENDIAN 1
#elif V8_TARGET_ARCH_LOONG64
#define V8_TARGET_LITTLE_ENDIAN 1
#elif V8_TARGET_ARCH_MIPS
#if defined(__MIPSEB__)
#define V8_TARGET_BIG_ENDIAN 1
#else
#define V8_TARGET_LITTLE_ENDIAN 1
#endif
#elif V8_TARGET_ARCH_MIPS64
#if defined(__MIPSEB__) || defined(V8_TARGET_ARCH_MIPS64_BE)
#define V8_TARGET_BIG_ENDIAN 1
#else
#define V8_TARGET_LITTLE_ENDIAN 1
#endif
#elif __BIG_ENDIAN__  // FOR PPCGR on AIX
#define V8_TARGET_BIG_ENDIAN 1
#elif V8_TARGET_ARCH_PPC_LE
#define V8_TARGET_LITTLE_ENDIAN 1
#elif V8_TARGET_ARCH_PPC_BE
#define V8_TARGET_BIG_ENDIAN 1
#elif V8_TARGET_ARCH_S390
#if V8_TARGET_ARCH_S390_LE_SIM
#define V8_TARGET_LITTLE_ENDIAN 1
#else
#define V8_TARGET_BIG_ENDIAN 1
#endif
#elif V8_TARGET_ARCH_RISCV64
#define V8_TARGET_LITTLE_ENDIAN 1
#else
#error Unknown target architecture endianness
#endif

// pthread_jit_write_protect is only available on arm64 Mac.
#if defined(V8_OS_MACOS) && defined(V8_HOST_ARCH_ARM64)
#define V8_HAS_PTHREAD_JIT_WRITE_PROTECT 1
#else
#define V8_HAS_PTHREAD_JIT_WRITE_PROTECT 0
#endif

#if defined(V8_TARGET_ARCH_IA32) || defined(V8_TARGET_ARCH_X64)
#define V8_TARGET_ARCH_STORES_RETURN_ADDRESS_ON_STACK true
#else
#define V8_TARGET_ARCH_STORES_RETURN_ADDRESS_ON_STACK false
#endif
constexpr int kReturnAddressStackSlotCount =
    V8_TARGET_ARCH_STORES_RETURN_ADDRESS_ON_STACK ? 1 : 0;

// Number of bits to represent the page size for paged spaces.
#if (defined(V8_HOST_ARCH_PPC) || defined(V8_HOST_ARCH_PPC64)) && !defined(_AIX)
// Native PPC linux has large (64KB) physical pages.
// Simulator (and Aix) need to use the same value as x64.
const int kPageSizeBits = 19;
#elif defined(ENABLE_HUGEPAGE)
// When enabling huge pages, adjust V8 page size to take up exactly one huge
// page. This avoids huge-page-internal fragmentation for unused address ranges.
const int kHugePageBits = 21;
const int kHugePageSize = (1U) << kHugePageBits;
const int kPageSizeBits = kHugePageBits;
#else
// Arm64 supports up to 64k OS pages on Linux, however 4k pages are more common
// so we keep the V8 page size at 256k. Nonetheless, we need to make sure we
// don't decrease it further in the future due to reserving 3 OS pages for every
// executable V8 page.
const int kPageSizeBits = 18;
#endif

#endif  // V8_BASE_BUILD_CONFIG_H_
