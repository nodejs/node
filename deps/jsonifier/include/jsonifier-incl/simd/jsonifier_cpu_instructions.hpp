/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/simd/jsonifier_cpu_instructions.hpp
 */
#pragma once

#undef JSONIFIER_SVE2_VECTOR_BITS
#define JSONIFIER_SVE2_VECTOR_BITS 0

#if !defined(JSONIFIER_LZCNT)
	#define JSONIFIER_LZCNT 1
#endif

#if !defined(JSONIFIER_POPCNT)
	#define JSONIFIER_POPCNT 1
#endif

#if !defined(JSONIFIER_BMI)
	#define JSONIFIER_BMI 1
#endif

#if !defined(JSONIFIER_CLMUL)
	#define JSONIFIER_CLMUL 1
#endif

#if !defined(JSONIFIER_NEON)
	#define JSONIFIER_NEON 0
#endif

#if !defined(JSONIFIER_AVX)
	#define JSONIFIER_AVX 1
#endif

#if !defined(JSONIFIER_AVX2)
	#define JSONIFIER_AVX2 1
#endif

#if !defined(JSONIFIER_AVX512)
	#define JSONIFIER_AVX512 0
#endif

#if !defined(JSONIFIER_SVE2)
	#define JSONIFIER_SVE2 0
#endif

#if !defined(JSONIFIER_ANY_AVX)
	#define JSONIFIER_ANY_AVX (JSONIFIER_AVX || JSONIFIER_AVX2 || JSONIFIER_AVX512)
#endif

#if !defined(JSONIFIER_ANY_SIMD)
	#define JSONIFIER_ANY_SIMD (JSONIFIER_AVX || JSONIFIER_AVX2 || JSONIFIER_AVX512 || JSONIFIER_NEON || JSONIFIER_SVE2)
#endif

#if JSONIFIER_NEON && JSONIFIER_SVE2
	#error "JSONIFIER_NEON and JSONIFIER_SVE2 are mutually exclusive backends - every SVE2 part also reports NEON, so exactly one must be selected."
#endif

#if JSONIFIER_SVE2 && JSONIFIER_SVE2_VECTOR_BITS == 0
	#error "JSONIFIER_SVE2 is selected but JSONIFIER_SVE2_VECTOR_BITS is 0 - the fixed-length SVE2 typedefs require a measured vector length."
#endif
