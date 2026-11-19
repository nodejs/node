/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/simd/neon_stage1.hpp
 */
// Sampled from Dr. Lemire's library, simdjson: https://github.com/simdjson/simdjson
#pragma once

#include <jsonifier-incl/simd/neon.hpp>

namespace jsonifier::internal::simd {

#if JSONIFIER_NEON

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
	#if JSONIFIER_CLMUL && JSONIFIER_COMPILER_CLANG
			return vgetq_lane_u64(vreinterpretq_u64_p128(vmull_p64(~0ULL, bitmask)), 0);
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

	struct unescaped_collector {
		JSONIFIER_INLINE static uint64_t impl(const simd_array_t in01) noexcept {
			static constexpr uint8x16_t bitMask{ 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
			uint8x16_t sum0 = vpaddq_u8(vandq_u8(in01.template get<0>(), bitMask), vandq_u8(in01.template get<1>(), bitMask));
			uint8x16_t sum1 = vpaddq_u8(vandq_u8(in01.template get<2>(), bitMask), vandq_u8(in01.template get<3>(), bitMask));
			sum0			= vpaddq_u8(sum0, sum1);
			sum0			= vpaddq_u8(sum0, sum0);
			return vgetq_lane_u64(vreinterpretq_u64_u8(sum0), 0);
		}
	};

	struct ws_collector {
		JSONIFIER_INLINE static uint64_t impl(const simd_array_t in01, const jsonifier_simd_int_t whitespaceTableLocal) noexcept {
			const uint8x16_t d00	  = in01.template get<0>();
			const uint8x16_t d01	  = in01.template get<1>();
			const uint8x16_t d02	  = in01.template get<2>();
			const uint8x16_t d03	  = in01.template get<3>();
			const uint8x16_t matchWs0 = vqtbx1q_u8(vceqq_u8(d00, vdupq_n_u8(uint_c_v<' '>)), whitespaceTableLocal, d00);
			const uint8x16_t matchWs1 = vqtbx1q_u8(vceqq_u8(d01, vdupq_n_u8(uint_c_v<' '>)), whitespaceTableLocal, d01);
			const uint8x16_t matchWs2 = vqtbx1q_u8(vceqq_u8(d02, vdupq_n_u8(uint_c_v<' '>)), whitespaceTableLocal, d02);
			const uint8x16_t matchWs3 = vqtbx1q_u8(vceqq_u8(d03, vdupq_n_u8(uint_c_v<' '>)), whitespaceTableLocal, d03);
			static constexpr uint8x16_t bitMask{ 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
			uint8x16_t wsSum0 = vpaddq_u8(vandq_u8(matchWs0, bitMask), vandq_u8(matchWs1, bitMask));
			uint8x16_t wsSum1 = vpaddq_u8(vandq_u8(matchWs2, bitMask), vandq_u8(matchWs3, bitMask));
			wsSum0			  = vpaddq_u8(wsSum0, wsSum1);
			wsSum0			  = vpaddq_u8(wsSum0, wsSum0);
			return vgetq_lane_u64(vreinterpretq_u64_u8(wsSum0), 0);
		}
	};

	struct op_collector {
		JSONIFIER_INLINE static uint64_t impl(const simd_array_t in01, const jsonifier_simd_int_t opTable, const jsonifier_simd_int_t) noexcept {
			const uint8x16_t d00	  = in01.template get<0>();
			const uint8x16_t d01	  = in01.template get<1>();
			const uint8x16_t d02	  = in01.template get<2>();
			const uint8x16_t d03	  = in01.template get<3>();
			const uint8x16_t matchOp0 = vceqq_u8(vqtbl1q_u8(opTable, vshrq_n_u8(vaddq_u8(d00, vdupq_n_u8(3)), 4)), d00);
			const uint8x16_t matchOp1 = vceqq_u8(vqtbl1q_u8(opTable, vshrq_n_u8(vaddq_u8(d01, vdupq_n_u8(3)), 4)), d01);
			const uint8x16_t matchOp2 = vceqq_u8(vqtbl1q_u8(opTable, vshrq_n_u8(vaddq_u8(d02, vdupq_n_u8(3)), 4)), d02);
			const uint8x16_t matchOp3 = vceqq_u8(vqtbl1q_u8(opTable, vshrq_n_u8(vaddq_u8(d03, vdupq_n_u8(3)), 4)), d03);
			static constexpr uint8x16_t bitMask{ 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
			uint8x16_t opSum0 = vpaddq_u8(vandq_u8(matchOp0, bitMask), vandq_u8(matchOp1, bitMask));
			uint8x16_t opSum1 = vpaddq_u8(vandq_u8(matchOp2, bitMask), vandq_u8(matchOp3, bitMask));
			opSum0			  = vpaddq_u8(opSum0, opSum1);
			opSum0			  = vpaddq_u8(opSum0, opSum0);
			return vgetq_lane_u64(vreinterpretq_u64_u8(opSum0), 0);
		}
	};

	struct bracket_masks {
		uint64_t closes;
		uint64_t opens;
	};

	struct bracket_collector {
		JSONIFIER_INLINE static uint8x16_t partialBitmask(const uint8x16_t f0, const uint8x16_t f1, const uint8x16_t f2, const uint8x16_t f3, const uint8x16_t target) noexcept {
			static constexpr uint8x16_t bitMask{ 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
			const uint8x16_t sum0 = vpaddq_u8(vandq_u8(vceqq_u8(f0, target), bitMask), vandq_u8(vceqq_u8(f1, target), bitMask));
			const uint8x16_t sum1 = vpaddq_u8(vandq_u8(vceqq_u8(f2, target), bitMask), vandq_u8(vceqq_u8(f3, target), bitMask));
			return vpaddq_u8(sum0, sum1);
		}

		JSONIFIER_INLINE static bracket_masks impl(const simd_array_t in01, const uint8x16_t spaceMask, const uint8x16_t openRegister, const uint8x16_t closeRegister) noexcept {
			const uint8x16_t f0			 = vorrq_u8(in01.template get<0>(), spaceMask);
			const uint8x16_t f1			 = vorrq_u8(in01.template get<1>(), spaceMask);
			const uint8x16_t f2			 = vorrq_u8(in01.template get<2>(), spaceMask);
			const uint8x16_t f3			 = vorrq_u8(in01.template get<3>(), spaceMask);
			const uint64x2_t pairedMasks = vreinterpretq_u64_u8(vpaddq_u8(partialBitmask(f0, f1, f2, f3, openRegister), partialBitmask(f0, f1, f2, f3, closeRegister)));
			return { vgetq_lane_u64(pairedMasks, 1), vgetq_lane_u64(pairedMasks, 0) };
		}
	};

	struct ws_op_collector {
		JSONIFIER_INLINE static uint64x2_t impl(const simd_array_t in01, const jsonifier_simd_int_t opTable, const jsonifier_simd_int_t,
			const jsonifier_simd_int_t whitespaceTableLocal) noexcept {
			static constexpr uint8x16_t bitMask{ 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
			const uint8x16_t d00	  = in01.template get<0>();
			const uint8x16_t d01	  = in01.template get<1>();
			const uint8x16_t d02	  = in01.template get<2>();
			const uint8x16_t d03	  = in01.template get<3>();
			const uint8x16_t matchWs0 = vqtbx1q_u8(vceqq_u8(d00, vdupq_n_u8(uint_c_v<' '>)), whitespaceTableLocal, d00);
			const uint8x16_t matchWs1 = vqtbx1q_u8(vceqq_u8(d01, vdupq_n_u8(uint_c_v<' '>)), whitespaceTableLocal, d01);
			const uint8x16_t matchWs2 = vqtbx1q_u8(vceqq_u8(d02, vdupq_n_u8(uint_c_v<' '>)), whitespaceTableLocal, d02);
			const uint8x16_t matchWs3 = vqtbx1q_u8(vceqq_u8(d03, vdupq_n_u8(uint_c_v<' '>)), whitespaceTableLocal, d03);
			const uint8x16_t matchOp0 = vceqq_u8(vqtbl1q_u8(opTable, vshrq_n_u8(vaddq_u8(d00, vdupq_n_u8(3)), 4)), d00);
			const uint8x16_t matchOp1 = vceqq_u8(vqtbl1q_u8(opTable, vshrq_n_u8(vaddq_u8(d01, vdupq_n_u8(3)), 4)), d01);
			const uint8x16_t matchOp2 = vceqq_u8(vqtbl1q_u8(opTable, vshrq_n_u8(vaddq_u8(d02, vdupq_n_u8(3)), 4)), d02);
			const uint8x16_t matchOp3 = vceqq_u8(vqtbl1q_u8(opTable, vshrq_n_u8(vaddq_u8(d03, vdupq_n_u8(3)), 4)), d03);
			const uint8x16_t wsSum =
				vpaddq_u8(vpaddq_u8(vandq_u8(matchWs0, bitMask), vandq_u8(matchWs1, bitMask)), vpaddq_u8(vandq_u8(matchWs2, bitMask), vandq_u8(matchWs3, bitMask)));
			const uint8x16_t opSum =
				vpaddq_u8(vpaddq_u8(vandq_u8(matchOp0, bitMask), vandq_u8(matchOp1, bitMask)), vpaddq_u8(vandq_u8(matchOp2, bitMask), vandq_u8(matchOp3, bitMask)));
			return vreinterpretq_u64_u8(vpaddq_u8(wsSum, opSum));
		}
	};

	template<uint64_t registerCount> struct scalar_bitmask_collector {
		using simd_array_type = pod_simd_array_t<registerCount, 16>;

		JSONIFIER_INLINE static uint64_t impl(const simd_array_type matches) noexcept {
			static constexpr uint8x16_t bitMask{ 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
			if constexpr (registerCount == 1) {
				const uint8x16_t masked = vandq_u8(matches.template get<0>(), bitMask);
				uint8x16_t sum			= vpaddq_u8(masked, masked);
				sum						= vpaddq_u8(sum, sum);
				sum						= vpaddq_u8(sum, sum);
				return static_cast<uint64_t>(vgetq_lane_u16(vreinterpretq_u16_u8(sum), 0));
			} else {
				uint8x16_t sum = vpaddq_u8(vandq_u8(matches.template get<0>(), bitMask), vandq_u8(matches.template get<1>(), bitMask));
				sum			   = vpaddq_u8(sum, sum);
				sum			   = vpaddq_u8(sum, sum);
				return static_cast<uint64_t>(vgetq_lane_u32(vreinterpretq_u32_u8(sum), 0));
			}
		}

		static_assert(registerCount == 1 || registerCount == 2);
	};

	template<uint64_t registerBytes, uint64_t registerCount> struct pod_cmp_eq_op {
		using simd_array_type = pod_simd_array_t<registerCount, registerBytes>;

		JSONIFIER_INLINE static uint64_t impl(const simd_array_type in01, const uint8x16_t rhsBroadcast) noexcept {
			simd_array_type matches;
			matches.template set<0>(vceqq_u8(in01.template get<0>(), rhsBroadcast));
			if constexpr (registerCount > 1) {
				matches.template set<1>(vceqq_u8(in01.template get<1>(), rhsBroadcast));
			}
			return scalar_bitmask_collector<registerCount>::impl(matches);
		}

		static_assert(registerBytes == 16 && (registerCount == 1 || registerCount == 2));
	};

	template<uint64_t registerBytes, uint64_t registerCount> struct pod_ws_collector {
		using simd_array_type = pod_simd_array_t<registerCount, registerBytes>;

		JSONIFIER_INLINE static uint64_t impl(const simd_array_type in01, const uint8x16_t whitespaceTableLocal) noexcept {
			if constexpr (registerCount == simdRegistersPerBlock) {
				return ws_collector::impl(in01, whitespaceTableLocal);
			} else {
				simd_array_type matches;
				const uint8x16_t d00 = in01.template get<0>();
				matches.template set<0>(vqtbx1q_u8(vceqq_u8(d00, vdupq_n_u8(uint_c_v<' '>)), whitespaceTableLocal, d00));
				if constexpr (registerCount > 1) {
					const uint8x16_t d01 = in01.template get<1>();
					matches.template set<1>(vqtbx1q_u8(vceqq_u8(d01, vdupq_n_u8(uint_c_v<' '>)), whitespaceTableLocal, d01));
				}
				return scalar_bitmask_collector<registerCount>::impl(matches);
			}
		}
	};

	template<uint64_t registerBytes, uint64_t registerCount> struct scalar_op_collector {
		using simd_array_type = pod_simd_array_t<registerCount, registerBytes>;

		JSONIFIER_INLINE static uint64_t impl(const simd_array_type in01, const uint8x16_t opTable, const uint8x16_t spaceMask) noexcept {
			if constexpr (registerCount == simdRegistersPerBlock) {
				return op_collector::impl(in01, opTable, spaceMask);
			} else {
				simd_array_type matches;
				const uint8x16_t d00 = in01.template get<0>();
				matches.template set<0>(vceqq_u8(vqtbl1q_u8(opTable, vshrq_n_u8(vaddq_u8(d00, vdupq_n_u8(3)), 4)), d00));
				if constexpr (registerCount > 1) {
					const uint8x16_t d01 = in01.template get<1>();
					matches.template set<1>(vceqq_u8(vqtbl1q_u8(opTable, vshrq_n_u8(vaddq_u8(d01, vdupq_n_u8(3)), 4)), d01));
				}
				return scalar_bitmask_collector<registerCount>::impl(matches);
			}
		}
	};

	template<typename rope_block> struct rope_detector : rope_block {
		JSONIFIER_INLINE void next(const simd_array_t in01, const jsonifier_simd_int_t bsRegister, const jsonifier_simd_int_t quoteRegister) noexcept {
			static constexpr uint8x16_t bitMask{ 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
			const uint8x16_t d0				  = in01.template get<0>();
			const uint8x16_t d1				  = in01.template get<1>();
			const uint8x16_t d2				  = in01.template get<2>();
			const uint8x16_t d3				  = in01.template get<3>();
			const uint8x16_t backslashPartial = toPartialBitmask(vceqq_u8(d0, bsRegister), vceqq_u8(d1, bsRegister), vceqq_u8(d2, bsRegister), vceqq_u8(d3, bsRegister), bitMask);
			const uint8x16_t quotesPartial =
				toPartialBitmask(vceqq_u8(d0, quoteRegister), vceqq_u8(d1, quoteRegister), vceqq_u8(d2, quoteRegister), vceqq_u8(d3, quoteRegister), bitMask);
			const uint64x2_t pairedMasks  = vreinterpretq_u64_u8(vpaddq_u8(backslashPartial, quotesPartial));
			const uint64_t backslashLocal = vgetq_lane_u64(pairedMasks, 0);
			const uint64_t quotesLocal	  = vgetq_lane_u64(pairedMasks, 1);
			const uint64_t escaped		  = nextEscapeAndTerminalCode(backslashLocal);
			const uint64_t quotes		  = (quotesLocal & ~escaped);
			rope_block::escaped			  = escaped;
			rope_block::quotes			  = quotes;
			return quotes ? finishNextInString() : finishNextNoInString();
		}

		JSONIFIER_INLINE static uint8x16_t toPartialBitmask(const uint8x16_t match0, const uint8x16_t match1, const uint8x16_t match2, const uint8x16_t match3,
			const uint8x16_t bitMask) noexcept {
			const uint8x16_t sum0 = vpaddq_u8(vandq_u8(match0, bitMask), vandq_u8(match1, bitMask));
			const uint8x16_t sum1 = vpaddq_u8(vandq_u8(match2, bitMask), vandq_u8(match3, bitMask));
			return vpaddq_u8(sum0, sum1);
		}

		template<uint64_t registerBytes, uint64_t registerCount> JSONIFIER_INLINE void nextScalar(const pod_simd_array_t<registerCount, registerBytes> in01,
			const simd_register_t<registerBytes> bsRegister, const simd_register_t<registerBytes> quoteRegister) noexcept {
			if constexpr (registerCount == simdRegistersPerBlock) {
				next(in01, bsRegister, quoteRegister);
			} else {
				const uint64_t escaped = nextEscapeAndTerminalCode(pod_cmp_eq_op<registerBytes, registerCount>::impl(in01, bsRegister));
				const uint64_t quotes  = (pod_cmp_eq_op<registerBytes, registerCount>::impl(in01, quoteRegister) & ~escaped);
				rope_block::escaped	   = escaped;
				rope_block::quotes	   = quotes;
				return quotes ? finishNextInString() : finishNextNoInString();
			}
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

		JSONIFIER_INLINE void finishNextInString() noexcept {
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

	struct tape_writer_op {
		JSONIFIER_INLINE static uint32_t extractIndex(const uint64_t base, const uint64_t bits) noexcept {
			return static_cast<uint32_t>(countrZero(bits) + base);
		}

		JSONIFIER_INLINE static uint64_t correctedPopcount(const uint64_t bits) noexcept {
			return static_cast<uint64_t>(popcount(bits));
		}

		JSONIFIER_INLINE static uint64_t advance(const uint64_t bits) noexcept {
			return blsr(bits);
		}
	};

	template<uint64_t size> consteval array<uint8_t, size> generateWhitespaceArrayNeon() noexcept {
		constexpr const uint8_t values[]{ 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xFFu, 0xFFu, 0x00u, 0x00u, 0xFFu, 0x00u, 0x00u };
		array<uint8_t, size> returnValues{};
		for (uint64_t x = 0; x < size; ++x) {
			returnValues[x] = values[x % 16];
		}
		return returnValues;
	};

	template<uint64_t size> alignas(simdBytesPerRegister) inline constexpr array<uint8_t, size> whitespaceArray{ generateWhitespaceArrayNeon<size>() };

	template<uint64_t size> consteval array<uint8_t, size> generateOpArrayNeon() noexcept {
		constexpr const uint8_t values[]{ 0xFFu, 0x00u, uint_c_v<','>, uint_c_v<':'>, 0x00u, uint_c_v<'['>, uint_c_v<']'>, uint_c_v<'{'>, uint_c_v<'}'>, 0x00u, 0x00u, 0x00u,
			0x00u, 0x00u, 0x00u, 0x00u };
		array<uint8_t, size> returnValues{};
		for (uint64_t x = 0; x < size; ++x) {
			returnValues[x] = values[x % 16];
		}
		return returnValues;
	};

	template<uint64_t size> alignas(simdBytesPerRegister) inline constexpr array<uint8_t, size> opArray{ generateOpArrayNeon<size>() };

#endif

}
