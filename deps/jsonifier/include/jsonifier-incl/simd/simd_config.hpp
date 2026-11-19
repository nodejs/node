/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/simd/simd_config.hpp
 */
#pragma once

#include <jsonifier-incl/core/config.hpp>
#include <jsonifier-incl/simd/simd_x.hpp>

namespace jsonifier::internal {

	using read_structural_index_ptr	 = const uint32_t* __restrict;
	using read_buffer_ptr			 = const uint8_t* __restrict;
	using write_structural_index_ptr = uint32_t* __restrict;
	using write_buffer_ptr			 = uint8_t* __restrict;

#if JSONIFIER_AVX512
	inline constexpr uint64_t simdBlocksPerStep{ 8 };
#elif JSONIFIER_AVX2
	inline constexpr uint64_t simdTapeStep{ 4 };
	#if JSONIFIER_COMPILER_CLANG
	inline constexpr uint64_t simdBlocksPerStep{ 4 };
	#else
	inline constexpr uint64_t simdBlocksPerStep{ 8 };
	#endif
#elif JSONIFIER_NEON
	inline constexpr uint64_t simdTapeStep{ 8 };
	inline constexpr uint64_t simdBlocksPerStep{ 4 };
#else
	inline constexpr uint64_t simdTapeStep{ 8 };
	inline constexpr uint64_t simdBlocksPerStep{ 8 };
#endif

#if JSONIFIER_ANY_AVX

	using jsonifier_simd_int_128 = __m128i;
	using jsonifier_simd_int_256 = __m256i;
	using jsonifier_simd_int_512 = __m512i;

	#if JSONIFIER_AVX512
	inline constexpr const char* cpuArchNameInternal{ "AVX512" };
	using jsonifier_simd_int_t = jsonifier_simd_int_512;
	#elif JSONIFIER_AVX2
	inline constexpr const char* cpuArchNameInternal{ "AVX2" };
	using jsonifier_simd_int_t = jsonifier_simd_int_256;
	#elif JSONIFIER_AVX
	inline constexpr const char* cpuArchNameInternal{ "AVX" };
	using jsonifier_simd_int_t = jsonifier_simd_int_128;
	#endif

#elif JSONIFIER_SVE2

	static_assert(JSONIFIER_SVE2_VECTOR_BITS == 128, "Jsonifier's SVE2 path is only implemented for a 128-bit vector length.");

	using jsonifier_simd_int_128 = svuint8_t __attribute__((arm_sve_vector_bits(JSONIFIER_SVE2_VECTOR_BITS)));
	using jsonifier_simd_int_256 = uint32_t;
	using jsonifier_simd_int_512 = uint64_t;

	inline constexpr const char* cpuArchNameInternal{ "SVE2" };
	using jsonifier_simd_int_t = jsonifier_simd_int_128;

#elif JSONIFIER_NEON

	using jsonifier_simd_int_128 = uint8x16_t;
	using jsonifier_simd_int_256 = uint32_t;
	using jsonifier_simd_int_512 = uint64_t;

	inline constexpr const char* cpuArchNameInternal{ "NEON" };
	using jsonifier_simd_int_t = jsonifier_simd_int_128;

#else

	using jsonifier_simd_int_128 = internal::simd::simd_x;
	using jsonifier_simd_int_256 = uint32_t;
	using jsonifier_simd_int_512 = uint64_t;

	inline constexpr const char* cpuArchNameInternal{ "FALLBACK" };
	using jsonifier_simd_int_t = jsonifier_simd_int_128;

#endif

	inline constexpr uint64_t simdBytesPerRegister{ internal::cpu_properties::getValue(internal::cpu_property_types::alignment) };
	inline constexpr uint64_t simdRegistersPerBlock{ 64 / simdBytesPerRegister };
	inline constexpr uint64_t simdBytesPerBlock{ 64 };
	inline constexpr uint64_t simdBytesPerStep = simdBlocksPerStep * simdBytesPerBlock;

#if JSONIFIER_AVX2 && !JSONIFIER_AVX512 && JSONIFIER_COMPILER_GCC
	inline constexpr uint64_t podBlocksPerStep{ 4 };
#else
	inline constexpr uint64_t podBlocksPerStep{ simdBlocksPerStep };
#endif
	inline constexpr uint64_t podBytesPerStep = podBlocksPerStep * simdBytesPerBlock;

	static_assert(simdBytesPerRegister == sizeof(jsonifier_simd_int_t),
		"simdBytesPerRegister disagrees with the actual register width; simdRegistersPerBlock and every bitmask collapse depend on these matching.");
	static_assert(simdBytesPerBlock % simdBytesPerRegister == 0, "Register width must evenly divide the 64-byte block.");

	template<uint64_t registerBytes> struct simd_register_selector {
		using type = void;
	};

	template<> struct simd_register_selector<16> {
		using type = jsonifier_simd_int_128;
	};

	template<> struct simd_register_selector<32> {
		using type = jsonifier_simd_int_256;
	};

	template<> struct simd_register_selector<64> {
		using type = jsonifier_simd_int_512;
	};

	template<uint64_t registerBytes> using simd_register_t = typename simd_register_selector<registerBytes>::type;

	template<uint64_t registerCount, uint64_t registerBytes = simdBytesPerRegister> struct simd_register_array {
		using simd_type = simd_register_t<registerBytes>;

		template<uint64_t indexNew> JSONIFIER_INLINE void set(const simd_type value) noexcept {
			static_assert(indexNew < registerCount, "simd_register_array::set index out of range.");
			values[indexNew] = value;
		}

		template<uint64_t indexNew> JSONIFIER_INLINE simd_type get() const noexcept {
			static_assert(indexNew < registerCount, "simd_register_array::get index out of range.");
			return values[indexNew];
		}

		alignas(registerBytes) simd_type values[registerCount];
	};

	template<uint64_t registerCount> using simd_array = simd_register_array<registerCount>;

	using simd_array_t = simd_array<simdRegistersPerBlock>;

	template<uint64_t registerCount, uint64_t registerBytes> using pod_simd_array_t = simd_register_array<registerCount, registerBytes>;
}

namespace jsonifier {

	inline constexpr const char* cpuArchName{ internal::cpuArchNameInternal };

}
