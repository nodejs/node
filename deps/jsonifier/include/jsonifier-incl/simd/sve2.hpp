/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/simd/sve2.hpp
 */
#pragma once

#include <jsonifier-incl/simd/simd_config.hpp>
#include <jsonifier-incl/simd/bit_ops.hpp>

namespace jsonifier::internal::simd {

#if JSONIFIER_SVE2

	template<int32_t alignment, simd_int_sve2_types simd_int_t01> [[maybe_unused]] JSONIFIER_INLINE jsonifier_simd_int_t opSrLi(simd_int_t01 value) noexcept {
		return svlsr_n_u8_x(svptrue_b8(), value, alignment);
	}

	template<simd_int_sve2_types simd_int_type_new> [[maybe_unused]] JSONIFIER_INLINE void storeU(simd_int_type_new value, void* storageLocation) noexcept {
		svst1_u8(svptrue_b8(), static_cast<uint8_t*>(storageLocation), value);
	}

	template<simd_int_sve2_types simd_int_type_new> [[maybe_unused]] JSONIFIER_INLINE void store(simd_int_type_new value, void* storageLocation) noexcept {
		svst1_u8(svptrue_b8(), static_cast<uint8_t*>(storageLocation), value);
	}

	template<simd_int_sve2_types simd_int_type_new> [[maybe_unused]] JSONIFIER_INLINE simd_int_type_new gatherValuesU(const void* str) noexcept {
		return svld1_u8(svptrue_b8(), static_cast<read_buffer_ptr>(str));
	}

	template<simd_int_sve2_types simd_int_type_new> [[maybe_unused]] JSONIFIER_INLINE simd_int_type_new gatherValues(const void* str) noexcept {
		return svld1_u8(svptrue_b8(), static_cast<read_buffer_ptr>(str));
	}

	template<simd_int_sve2_types simd_int_t01> [[maybe_unused]] JSONIFIER_INLINE uint64_t opBitMaskRaw(simd_int_t01 value) noexcept {
		static_assert(JSONIFIER_SVE2_VECTOR_BITS == 128, "opBitMaskRaw's nibble-per-byte packing only fits a uint64_t at 128-bit VL.");
		const jsonifier_simd_int_t narrowed = svshrnb_n_u16(svreinterpret_u16_u8(value), 4);
		const jsonifier_simd_int_t packed	= svuzp1_u8(narrowed, narrowed);
		return svlastb_u64(svptrue_pat_b64(SV_VL1), svreinterpret_u64_u8(packed));
	}

	template<simd_int_sve2_types simd_int_t01> [[maybe_unused]] JSONIFIER_INLINE uint16_t opBitMask(simd_int_t01 value) noexcept {
		static_assert(JSONIFIER_SVE2_VECTOR_BITS == 128, "opBitMask returns uint16_t and therefore only covers a 128-bit vector.");
		const uint8x16_t masked = vandq_u8(svget_neonq_u8(value), vreinterpretq_u8_u64(vdupq_n_u64(0x8040201008040201ULL)));
		const uint64x2_t summed = vpaddlq_u32(vpaddlq_u16(vpaddlq_u8(masked)));
		return static_cast<uint16_t>(vgetq_lane_u64(summed, 0) | (vgetq_lane_u64(summed, 1) << 8));
	}

	template<uint_types value_type> [[maybe_unused]] JSONIFIER_INLINE uint64_t postCmpTzcntUnsafe(value_type value) noexcept {
		return countrZeroUnsafe(value) >> 2;
	}

	template<simd_int_sve2_types simd_int_t01> [[maybe_unused]] JSONIFIER_INLINE bool opTest(simd_int_t01 value) noexcept {
		const auto pg = svptrue_b8();
		return !svptest_any(pg, svcmpne_n_u8(pg, value, 0));
	}

	template<int32_t shift, simd_int_sve2_types simd_int_t01, simd_int_sve2_types simd_int_t02>
	[[maybe_unused]] JSONIFIER_INLINE jsonifier_simd_int_t opPrev(simd_int_t01 current, simd_int_t02 previous) noexcept {
		return svext_u8(previous, current, shift);
	}

	template<uint_types value_type> [[maybe_unused]] JSONIFIER_INLINE uint64_t postCmpTzcnt(value_type value) noexcept {
		return countrZero(value) >> 2;
	}

	template<simd_int_sve2_types simd_int_t01, simd_int_sve2_types simd_int_t02>
	[[maybe_unused]] JSONIFIER_INLINE jsonifier_simd_int_t opCmpEqRaw(simd_int_t01 value, simd_int_t02 other) noexcept {
		return svdup_n_u8_z(svcmpeq_u8(svptrue_b8(), value, other), 0xFF);
	}

	template<simd_int_sve2_types simd_int_t01, simd_int_sve2_types simd_int_t02>
	[[maybe_unused]] JSONIFIER_INLINE jsonifier_simd_int_t opCmpLtRaw(simd_int_t01 value, simd_int_t02 other) noexcept {
		return svdup_n_u8_z(svcmplt_u8(svptrue_b8(), value, other), 0xFF);
	}

	template<simd_int_sve2_types simd_int_t01, simd_int_sve2_types simd_int_t02>
	[[maybe_unused]] JSONIFIER_INLINE jsonifier_simd_int_t opShuffle(simd_int_t01 value, simd_int_t02 other) noexcept {
		const jsonifier_simd_int_t bitMask = svreinterpret_u8_u8(svdup_n_u8(0x0F));
		return svtbl_u8(value, svand_u8_x(svptrue_b8(), other, bitMask));
	}

	template<simd_int_sve2_types simd_int_t01, simd_int_sve2_types simd_int_t02>
	[[maybe_unused]] JSONIFIER_INLINE jsonifier_simd_int_t opSubs(simd_int_t01 value, simd_int_t02 other) noexcept {
		return svqsub_u8_x(svptrue_b8(), value, other);
	}

	template<simd_int_sve2_types simd_int_t01, simd_int_sve2_types simd_int_t02>
	[[maybe_unused]] JSONIFIER_INLINE jsonifier_simd_int_t opXor(simd_int_t01 value, simd_int_t02 other) noexcept {
		return sveor_u8_x(svptrue_b8(), value, other);
	}

	template<simd_int_sve2_types simd_int_t01, simd_int_sve2_types simd_int_t02>
	[[maybe_unused]] JSONIFIER_INLINE jsonifier_simd_int_t opAnd(simd_int_t01 value, simd_int_t02 other) noexcept {
		return svand_u8_x(svptrue_b8(), value, other);
	}

	template<simd_int_sve2_types simd_int_t01, simd_int_sve2_types simd_int_t02>
	[[maybe_unused]] JSONIFIER_INLINE jsonifier_simd_int_t opOr(simd_int_t01 value, simd_int_t02 other) noexcept {
		return svorr_u8_x(svptrue_b8(), value, other);
	}

	template<simd_int_sve2_types simd_int_t01, uint64_t totalChunks> JSONIFIER_INLINE simd_int_t01 orAll(simd_array<totalChunks> chunks) noexcept {
		return opOr(chunks.template get<0>(), opOr(chunks.template get<1>(), opOr(chunks.template get<2>(), chunks.template get<3>())));
	}

	template<simd_int_sve2_types simd_int_t01, simd_int_sve2_types simd_int_t02>
	[[maybe_unused]] JSONIFIER_INLINE uint64_t opCmpEq(simd_int_t01 value, simd_int_t02 other) noexcept {
		return opBitMaskRaw(opCmpEqRaw(value, other));
	}

	template<simd_int_sve2_types simd_int_t> JSONIFIER_INLINE bool isAscii(simd_int_t input) noexcept {
		const auto pg = svptrue_b8();
		return !svptest_any(pg, svcmpge_n_u8(pg, input, 0x80));
	}

	template<simd_int_sve2_types simd_int_type_new, typename char_t>
		requires(sizeof(char_t) == 8)
	[[maybe_unused]] JSONIFIER_INLINE simd_int_type_new gatherValue(char_t str) noexcept {
		return svreinterpret_u8_u64(svdup_n_u64(static_cast<uint64_t>(str)));
	}

	template<simd_int_sve2_types simd_int_type_new, typename char_t>
		requires(sizeof(char_t) == 1)
	[[maybe_unused]] JSONIFIER_INLINE simd_int_type_new gatherValue(char_t str) noexcept {
		return svdup_n_u8(static_cast<uint8_t>(str));
	}

#endif

}
