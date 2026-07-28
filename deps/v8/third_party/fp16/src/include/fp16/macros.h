#pragma once
#ifndef FP16_MACROS_H
#define FP16_MACROS_H

#ifndef FP16_USE_NATIVE_CONVERSION
	#if (defined(__INTEL_COMPILER) || defined(__GNUC__)) && defined(__F16C__)
		#define FP16_USE_NATIVE_CONVERSION 1
	#elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64)) && defined(__AVX2__)
		#define FP16_USE_NATIVE_CONVERSION 1
	#elif defined(_MSC_VER) && defined(_M_ARM64)
		#define FP16_USE_NATIVE_CONVERSION 1
	#elif defined(__GNUC__) && defined(__aarch64__)
		#define FP16_USE_NATIVE_CONVERSION 1
	#endif
	#if !defined(FP16_USE_NATIVE_CONVERSION)
		#define FP16_USE_NATIVE_CONVERSION 0
	#endif  // !defined(FP16_USE_NATIVE_CONVERSION)
#endif  // !define(FP16_USE_NATIVE_CONVERSION)

#ifndef FP16_USE_FLOAT16_TYPE
	#if !defined(__clang__) && !defined(__INTEL_COMPILER) && defined(__GNUC__) && (__GNUC__ >= 12)
		#if defined(__F16C__)
			#define FP16_USE_FLOAT16_TYPE 1
		#endif
	#endif
	#if !defined(FP16_USE_FLOAT16_TYPE)
		#define FP16_USE_FLOAT16_TYPE 0
	#endif  // !defined(FP16_USE_FLOAT16_TYPE)
#endif  // !defined(FP16_USE_FLOAT16_TYPE)

#ifndef FP16_USE_FP16_TYPE
	#if defined(__clang__)
		#if defined(__F16C__) || defined(__aarch64__)
			#define FP16_USE_FP16_TYPE 1
		#endif
	#elif defined(__GNUC__)
		#if defined(__aarch64__)
			#define FP16_USE_FP16_TYPE 1
		#endif
	#endif
	#if !defined(FP16_USE_FP16_TYPE)
		#define FP16_USE_FP16_TYPE 0
	#endif  // !defined(FP16_USE_FP16_TYPE)
#endif  // !defined(FP16_USE_FP16_TYPE)

#endif /* FP16_MACROS_H */
