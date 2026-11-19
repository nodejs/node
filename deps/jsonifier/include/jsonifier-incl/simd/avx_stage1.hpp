/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/simd/avx_stage1.hpp
 */
// Sampled from Dr. Lemire's library, simdjson: https://github.com/simdjson/simdjson
#pragma once

#include <jsonifier-incl/containers/array.hpp>
#include <jsonifier-incl/simd/bit_ops.hpp>
#include <jsonifier-incl/simd/avx.hpp>

namespace jsonifier::internal::simd {

#if !JSONIFIER_NEON && !JSONIFIER_SVE2

	inline constexpr array<uint64_t, simdRegistersPerBlock> shiftAmounts{ [] {
		array<uint64_t, simdRegistersPerBlock> returnValue{};
		for (uint64_t x = 0; x < simdRegistersPerBlock; ++x) {
			returnValue[x] = simdBytesPerRegister * x;
		}
		return returnValue;
	}() };

	consteval uint64_t getShiftAmount(const uint64_t index) noexcept {
		return shiftAmounts[index % shiftAmounts.size()];
	}

	struct prefix_xor_op {
		JSONIFIER_INLINE static uint64_t impl(uint64_t bitmask) noexcept {
	#if JSONIFIER_CLMUL
			const __m128i allOnes = _mm_set1_epi8(static_cast<char>(-1));
			const __m128i result  = _mm_clmulepi64_si128(_mm_set_epi64x(0, static_cast<int64_t>(bitmask)), allOnes, 0);
			return static_cast<uint64_t>(_mm_cvtsi128_si64(result));
	#else
			bitmask ^= bitmask << 1;
			bitmask ^= bitmask << 2;
			bitmask ^= bitmask << 4;
			bitmask ^= bitmask << 8;
			bitmask ^= bitmask << 16;
			bitmask ^= bitmask << 32;
			return bitmask;
	#endif
		}
	};

	struct cmp_eq_op {
		JSONIFIER_INLINE static uint64_t impl(const simd_array_t lhs, const jsonifier_simd_int_t rhsBroadcast) noexcept {
			uint64_t result = opCmpEq(lhs.get<0>(), rhsBroadcast);
			if constexpr (simdRegistersPerBlock > 1) {
				result |= static_cast<uint64_t>(opCmpEq(lhs.get<1>(), rhsBroadcast)) << getShiftAmount(1);
				if constexpr (simdRegistersPerBlock > 2) {
					result |= static_cast<uint64_t>(opCmpEq(lhs.get<2>(), rhsBroadcast)) << getShiftAmount(2);
					result |= static_cast<uint64_t>(opCmpEq(lhs.get<3>(), rhsBroadcast)) << getShiftAmount(3);
				}
			}
			return result;
		}

		JSONIFIER_INLINE static uint64_t impl(const simd_array_t lhs, const simd_array_t rhs) noexcept {
			uint64_t result = opCmpEq(lhs.get<0>(), rhs.get<0>());
			if constexpr (simdRegistersPerBlock > 1) {
				result |= static_cast<uint64_t>(opCmpEq(lhs.get<1>(), rhs.get<1>())) << getShiftAmount(1);
				if constexpr (simdRegistersPerBlock > 2) {
					result |= static_cast<uint64_t>(opCmpEq(lhs.get<2>(), rhs.get<2>())) << getShiftAmount(2);
					result |= static_cast<uint64_t>(opCmpEq(lhs.get<3>(), rhs.get<3>())) << getShiftAmount(3);
				}
			}
			return result;
		}
	};

	struct unescaped_collector {
		JSONIFIER_INLINE static uint64_t impl(const simd_array_t ltRhs) noexcept {
			uint64_t result = static_cast<uint64_t>(opBitMask(ltRhs.get<0>()));
			if constexpr (simdRegistersPerBlock > 1) {
				result |= static_cast<uint64_t>(opBitMask(ltRhs.get<1>())) << getShiftAmount(1);
				if constexpr (simdRegistersPerBlock > 2) {
					result |= static_cast<uint64_t>(opBitMask(ltRhs.get<2>())) << getShiftAmount(2);
					result |= static_cast<uint64_t>(opBitMask(ltRhs.get<3>())) << getShiftAmount(3);
				}
			}
			return result;
		}
	};

	struct bracket_masks {
		uint64_t closes;
		uint64_t opens;
	};

	struct bracket_collector {
		JSONIFIER_INLINE static bracket_masks impl(const simd_array_t in01, const jsonifier_simd_int_t spaceMask, const jsonifier_simd_int_t openRegister,
			const jsonifier_simd_int_t closeRegister) noexcept {
			simd_array_t folded;
			folded.set<0>(opOr(in01.get<0>(), spaceMask));
			if constexpr (simdRegistersPerBlock > 1) {
				folded.set<1>(opOr(in01.get<1>(), spaceMask));
				if constexpr (simdRegistersPerBlock > 2) {
					folded.set<2>(opOr(in01.get<2>(), spaceMask));
					folded.set<3>(opOr(in01.get<3>(), spaceMask));
				}
			}
			return { cmp_eq_op::impl(folded, closeRegister), cmp_eq_op::impl(folded, openRegister) };
		}
	};

	template<typename rope_block> struct rope_detector : rope_block {
		template<uint64_t registerBytes, uint64_t registerCount> JSONIFIER_INLINE void nextScalar(const pod_simd_array_t<registerCount, registerBytes> in01,
			const simd_register_t<registerBytes> bsRegister, const simd_register_t<registerBytes> quoteRegister) noexcept;

		JSONIFIER_INLINE void next(const simd_array_t in01, const jsonifier_simd_int_t bsRegister, const jsonifier_simd_int_t quoteRegister) noexcept {
			const uint64_t escaped = nextEscapeAndTerminalCode(cmp_eq_op::impl(in01, bsRegister));
			const uint64_t quotes  = (cmp_eq_op::impl(in01, quoteRegister) & ~escaped);
			rope_block::escaped	   = escaped;
			rope_block::quotes	   = quotes;
			return quotes ? finishNext() : finishNextNoInString();
		}

		JSONIFIER_INLINE uint64_t nextEscapeAndTerminalCode(const uint64_t backslashLocal) noexcept {
			if (!backslashLocal) {
				const uint64_t escaped = nextIsEscaped;
				nextIsEscaped		   = 0;
				return escaped;
			}
			const uint64_t escapeAndTerminalCode = nextEscapeAndTerminalCodeImpl(backslashLocal & ~nextIsEscaped);
			const uint64_t escaped				 = escapeAndTerminalCode ^ (backslashLocal | nextIsEscaped);
			nextIsEscaped						 = (escapeAndTerminalCode & backslashLocal) >> 63;
			return escaped;
		}

		JSONIFIER_INLINE uint64_t nextEscapeAndTerminalCodeImpl(const uint64_t potentialEscape) noexcept {
			static constexpr uint64_t oddBits{ 0xAAAAAAAAAAAAAAAAULL };
			const uint64_t maybeEscaped				 = potentialEscape << 1;
			const uint64_t maybeEscapedAndOddBits	 = maybeEscaped | oddBits;
			const uint64_t evenSeriesCodesAndOddBits = maybeEscapedAndOddBits - potentialEscape;
			return evenSeriesCodesAndOddBits ^ oddBits;
		}

		JSONIFIER_INLINE uint64_t followsNonquoteScalar(const uint64_t nonquoteScalar) noexcept {
			const uint64_t shifted = (nonquoteScalar << 1) | prevScalar;
			prevScalar			   = nonquoteScalar >> 63;
			return shifted;
		}

		JSONIFIER_INLINE void finishNext() noexcept {
			const uint64_t inString = prefix_xor_op::impl(rope_block::quotes) ^ prevInString;
			prevInString			= static_cast<uint64_t>(static_cast<int64_t>(inString) >> 63);
			rope_block::inString	= inString;
		}

		JSONIFIER_INLINE void finishNextNoInString() noexcept {
			rope_block::inString = prevInString;
		}

		uint64_t nextIsEscaped{};
		uint64_t prevInString{};
		uint64_t prevScalar{};
	};

	struct ws_collector {
		JSONIFIER_INLINE static uint64_t impl(const simd_array_t in01, const jsonifier_simd_int_t whitespaceTableLocal) noexcept {
			simd_array_t wsShuffle;
			wsShuffle.set<0>(opShuffle(whitespaceTableLocal, in01.get<0>()));
			if constexpr (simdRegistersPerBlock > 1) {
				wsShuffle.set<1>(opShuffle(whitespaceTableLocal, in01.get<1>()));
				if constexpr (simdRegistersPerBlock > 2) {
					wsShuffle.set<2>(opShuffle(whitespaceTableLocal, in01.get<2>()));
					wsShuffle.set<3>(opShuffle(whitespaceTableLocal, in01.get<3>()));
				}
			}
			return cmp_eq_op::impl(in01, wsShuffle);
		}
	};

	struct op_collector {
		JSONIFIER_INLINE static uint64_t impl(const simd_array_t in01, const jsonifier_simd_int_t opTable, const jsonifier_simd_int_t spaceMask) noexcept {
			simd_array_t orLhs;
			simd_array_t shuffleRhs;

			orLhs.set<0>(opOr(in01.get<0>(), spaceMask));
			shuffleRhs.set<0>(opShuffle(opTable, in01.get<0>()));
			if constexpr (simdRegistersPerBlock > 1) {
				orLhs.set<1>(opOr(in01.get<1>(), spaceMask));
				shuffleRhs.set<1>(opShuffle(opTable, in01.get<1>()));
				if constexpr (simdRegistersPerBlock > 2) {
					orLhs.set<2>(opOr(in01.get<2>(), spaceMask));
					shuffleRhs.set<2>(opShuffle(opTable, in01.get<2>()));
					orLhs.set<3>(opOr(in01.get<3>(), spaceMask));
					shuffleRhs.set<3>(opShuffle(opTable, in01.get<3>()));
				}
			}
			return cmp_eq_op::impl(orLhs, shuffleRhs);
		}
	};

	template<uint64_t registerBytes, uint64_t registerCount> struct pod_cmp_eq_op {
		using simd_type		  = simd_register_t<registerBytes>;
		using simd_array_type = pod_simd_array_t<registerCount, registerBytes>;

		JSONIFIER_INLINE static uint64_t impl(const simd_array_type lhs, const simd_array_type rhs) noexcept {
			uint64_t result = static_cast<uint64_t>(opCmpEq(lhs.template get<0>(), rhs.template get<0>()));
			if constexpr (registerCount > 1) {
				result |= static_cast<uint64_t>(opCmpEq(lhs.template get<1>(), rhs.template get<1>())) << (registerBytes * 1);
				if constexpr (registerCount > 2) {
					result |= static_cast<uint64_t>(opCmpEq(lhs.template get<2>(), rhs.template get<2>())) << (registerBytes * 2);
					result |= static_cast<uint64_t>(opCmpEq(lhs.template get<3>(), rhs.template get<3>())) << (registerBytes * 3);
				}
			}
			return result;
		}

		JSONIFIER_INLINE static uint64_t impl(const simd_array_type lhs, const simd_type rhsBroadcast) noexcept {
			uint64_t result = static_cast<uint64_t>(opCmpEq(lhs.template get<0>(), rhsBroadcast));
			if constexpr (registerCount > 1) {
				result |= static_cast<uint64_t>(opCmpEq(lhs.template get<1>(), rhsBroadcast)) << (registerBytes * 1);
				if constexpr (registerCount > 2) {
					result |= static_cast<uint64_t>(opCmpEq(lhs.template get<2>(), rhsBroadcast)) << (registerBytes * 2);
					result |= static_cast<uint64_t>(opCmpEq(lhs.template get<3>(), rhsBroadcast)) << (registerBytes * 3);
				}
			}
			return result;
		}

		static_assert(registerCount >= 1 && registerCount * registerBytes <= simdBytesPerBlock);
	};

	template<uint64_t registerBytes, uint64_t registerCount> struct pod_ws_collector {
		using simd_type		  = simd_register_t<registerBytes>;
		using simd_array_type = pod_simd_array_t<registerCount, registerBytes>;

		JSONIFIER_INLINE static uint64_t impl(const simd_array_type in01, const simd_type whitespaceTableLocal) noexcept {
			simd_array_type wsShuffle;
			wsShuffle.template set<0>(opShuffle(whitespaceTableLocal, in01.template get<0>()));
			if constexpr (registerCount > 1) {
				wsShuffle.template set<1>(opShuffle(whitespaceTableLocal, in01.template get<1>()));
				if constexpr (registerCount > 2) {
					wsShuffle.template set<2>(opShuffle(whitespaceTableLocal, in01.template get<2>()));
					wsShuffle.template set<3>(opShuffle(whitespaceTableLocal, in01.template get<3>()));
				}
			}
			return pod_cmp_eq_op<registerBytes, registerCount>::impl(in01, wsShuffle);
		}
	};

	template<uint64_t registerBytes, uint64_t registerCount> struct scalar_op_collector {
		using simd_type		  = simd_register_t<registerBytes>;
		using simd_array_type = pod_simd_array_t<registerCount, registerBytes>;

		JSONIFIER_INLINE static uint64_t impl(const simd_array_type in01, const simd_type opTable, const simd_type spaceMask) noexcept {
			simd_array_type orLhs;
			simd_array_type shuffleRhs;

			orLhs.template set<0>(opOr(in01.template get<0>(), spaceMask));
			shuffleRhs.template set<0>(opShuffle(opTable, in01.template get<0>()));
			if constexpr (registerCount > 1) {
				orLhs.template set<1>(opOr(in01.template get<1>(), spaceMask));
				shuffleRhs.template set<1>(opShuffle(opTable, in01.template get<1>()));
				if constexpr (registerCount > 2) {
					orLhs.template set<2>(opOr(in01.template get<2>(), spaceMask));
					shuffleRhs.template set<2>(opShuffle(opTable, in01.template get<2>()));
					orLhs.template set<3>(opOr(in01.template get<3>(), spaceMask));
					shuffleRhs.template set<3>(opShuffle(opTable, in01.template get<3>()));
				}
			}
			return pod_cmp_eq_op<registerBytes, registerCount>::impl(orLhs, shuffleRhs);
		}
	};

	template<typename rope_block> template<uint64_t registerBytes, uint64_t registerCount> JSONIFIER_INLINE void rope_detector<rope_block>::nextScalar(
		const pod_simd_array_t<registerCount, registerBytes> in01, const simd_register_t<registerBytes> bsRegister, const simd_register_t<registerBytes> quoteRegister) noexcept {
		const uint64_t escaped = nextEscapeAndTerminalCode(pod_cmp_eq_op<registerBytes, registerCount>::impl(in01, bsRegister));
		const uint64_t quotes  = (pod_cmp_eq_op<registerBytes, registerCount>::impl(in01, quoteRegister) & ~escaped);
		rope_block::escaped	   = escaped;
		rope_block::quotes	   = quotes;
		return quotes ? finishNext() : finishNextNoInString();
	}

	struct tape_writer_op {
		JSONIFIER_INLINE static uint32_t extractIndex(const uint64_t base, const uint64_t bits) noexcept {
			return static_cast<uint32_t>(postCmpTzcnt(bits) + base);
		}

		JSONIFIER_INLINE static uint64_t correctedPopcount(const uint64_t bits) noexcept {
			return static_cast<uint64_t>(popcount(bits));
		}

		JSONIFIER_INLINE static uint64_t advance(const uint64_t bits) noexcept {
			return blsr(bits);
		}
	};

	template<uint64_t size> consteval array<uint8_t, size> generateWhitespaceArray() noexcept {
		constexpr const uint8_t values[]{ 0x20u, 0x64u, 0x64u, 0x64u, 0x11u, 0x64u, 0x71u, 0x02u, 0x64u, uint_c_v<'\t'>, uint_c_v<'\n'>, 0x70u, 0x64u, uint_c_v<'\r'>, 0x64u,
			0x64u };
		array<uint8_t, size> returnValues{};
		for (uint64_t x = 0; x < size; ++x) {
			returnValues[x] = values[x % 16];
		}
		return returnValues;
	};

	template<uint64_t size> alignas(simdBytesPerRegister) inline constexpr array<uint8_t, size> whitespaceArray{ generateWhitespaceArray<size>() };

	template<uint64_t size> consteval array<uint8_t, size> generateOpArray() noexcept {
		constexpr const uint8_t values[]{ 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, uint_c_v<':'>, uint_c_v<'{'>, uint_c_v<','>, uint_c_v<'}'>,
			0x00u, 0x00u };
		array<uint8_t, size> returnValues{};
		for (uint64_t x = 0; x < size; ++x) {
			returnValues[x] = values[x % 16];
		}
		return returnValues;
	};

	template<uint64_t size> alignas(simdBytesPerRegister) inline constexpr array<uint8_t, size> opArray{ generateOpArray<size>() };

#endif

}
