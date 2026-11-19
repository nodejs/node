/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/simd/utf8_validation.hpp
 */
// Sampled from Dr. Lemire's library, simdjson: https://github.com/simdjson/simdjson
#pragma once

#include <jsonifier-incl/utilities/utility.hpp>
#include <jsonifier-incl/containers/array.hpp>
#include <jsonifier-incl/simd/fallback.hpp>
#include <jsonifier-incl/simd/bit_ops.hpp>
#include <jsonifier-incl/simd/avx.hpp>
#include <jsonifier-incl/simd/neon.hpp>

namespace jsonifier::internal {

	inline constexpr uint8_t tooShort	  = 1 << 0;
	inline constexpr uint8_t tooLong	  = 1 << 1;
	inline constexpr uint8_t overLong3	  = 1 << 2;
	inline constexpr uint8_t tooLarge	  = 1 << 3;
	inline constexpr uint8_t surrogate	  = 1 << 4;
	inline constexpr uint8_t overLong2	  = 1 << 5;
	inline constexpr uint8_t twoConts	  = 1 << 7;
	inline constexpr uint8_t tooLarge1000 = 1 << 6;
	inline constexpr uint8_t overLong4	  = 1 << 6;
	inline constexpr uint8_t carry		  = tooShort | tooLong | twoConts;

	using namespace simd;

	consteval array<uint8_t, simdBytesPerRegister> genByte1HighTable() noexcept {
		constexpr array<uint8_t, 16ULL> raw{ { tooLong, tooLong, tooLong, tooLong, tooLong, tooLong, tooLong, tooLong, twoConts, twoConts, twoConts, twoConts,
			tooShort | overLong2, tooShort, tooShort | overLong3 | surrogate, tooShort | tooLarge | tooLarge1000 | overLong4 } };
		array<uint8_t, simdBytesPerRegister> returnValue{};
		for (uint64_t x = 0; x < simdBytesPerRegister; ++x) {
			returnValue[x] = raw[x % raw.size()];
		}
		return returnValue;
	}

	alignas(simdBytesPerRegister) inline constexpr auto byte1HighTableRaw{ genByte1HighTable() };

	inline constexpr read_buffer_ptr byte1HighTable{ byte1HighTableRaw.data() };

	consteval array<uint8_t, simdBytesPerRegister> genByte1LowTable() noexcept {
		constexpr array<uint8_t, 16ULL> raw{ { carry | overLong3 | overLong2 | overLong4, carry | overLong2, carry, carry, carry | tooLarge, carry | tooLarge | tooLarge1000,
			carry | tooLarge | tooLarge1000, carry | tooLarge | tooLarge1000, carry | tooLarge | tooLarge1000, carry | tooLarge | tooLarge1000, carry | tooLarge | tooLarge1000,
			carry | tooLarge | tooLarge1000, carry | tooLarge | tooLarge1000, carry | tooLarge | tooLarge1000 | surrogate, carry | tooLarge | tooLarge1000,
			carry | tooLarge | tooLarge1000 } };
		array<uint8_t, simdBytesPerRegister> returnValue{};
		for (uint64_t x = 0; x < simdBytesPerRegister; ++x) {
			returnValue[x] = raw[x % raw.size()];
		}
		return returnValue;
	}

	alignas(simdBytesPerRegister) inline constexpr auto byte1LowTableRaw{ genByte1LowTable() };

	inline constexpr read_buffer_ptr byte1LowTable{ byte1LowTableRaw.data() };

	consteval array<uint8_t, simdBytesPerRegister> genByte2HighTable() noexcept {
		constexpr array<uint8_t, 16ULL> raw{ { tooShort, tooShort, tooShort, tooShort, tooShort, tooShort, tooShort, tooShort,
			tooLong | overLong2 | twoConts | overLong3 | tooLarge1000 | overLong4, tooLong | overLong2 | twoConts | overLong3 | tooLarge,
			tooLong | overLong2 | twoConts | surrogate | tooLarge, tooLong | overLong2 | twoConts | surrogate | tooLarge, tooShort, tooShort, tooShort, tooShort } };
		array<uint8_t, simdBytesPerRegister> returnValue{};
		for (uint64_t x = 0; x < simdBytesPerRegister; ++x) {
			returnValue[x] = raw[x % raw.size()];
		}
		return returnValue;
	}

	alignas(simdBytesPerRegister) inline constexpr auto byte2HighTableRaw{ genByte2HighTable() };

	inline constexpr read_buffer_ptr byte2HighTable{ byte2HighTableRaw.data() };

	alignas(simdBytesPerRegister) inline constexpr uint8_t isIncompleteMax[64]{ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, static_cast<uint8_t>(0xF0u - 1), static_cast<uint8_t>(0xE0u - 1), static_cast<uint8_t>(0xC0u - 1) };

	template<typename integer_sequence> struct chunk_loader;

	template<uint64_t... indices> struct chunk_loader<integer_sequence<indices...>> {
		JSONIFIER_INLINE static simd_array_t impl(read_buffer_ptr src) noexcept {
			simd_array_t returnValues;
			(returnValues.template set<indices>(gatherValuesU<jsonifier_simd_int_t>(std::bit_cast<const jsonifier_simd_int_t* __restrict>(src + indices * simdBytesPerRegister))),
				...);
			return returnValues;
		}
	};

	template<typename derived_type, typename integer_sequence> struct step_checker;

	template<typename derived_type, uint64_t... indices> struct step_checker<derived_type, integer_sequence<indices...>> {
		JSONIFIER_INLINE void impl(read_buffer_ptr srcNew) noexcept {
			(static_cast<derived_type*>(this)->checkStepImpl(srcNew + indices * simdBytesPerBlock), ...);
		}
	};

	struct utf8_checker : public step_checker<utf8_checker, make_integer_sequence<simdBlocksPerStep>> {
		using step_checker_type = step_checker<utf8_checker, make_integer_sequence<simdBlocksPerStep>>;
		template<typename integer_sequence> struct chunk_processor;

		template<uint64_t... indices> struct chunk_processor<integer_sequence<indices...>> {
			template<uint64_t index> JSONIFIER_INLINE static void impl(utf8_checker& __restrict c, simd_array_t chunks, jsonifier_simd_int_t& __restrict prev) noexcept {
				c.checkChunk(chunks.template get<index>(), prev), prev = chunks.template get<index>();
			}

			JSONIFIER_INLINE static void impl(utf8_checker& __restrict c, simd_array_t chunks, jsonifier_simd_int_t& __restrict prev) noexcept {
				(impl<indices>(c, chunks, prev), ...);
			}
		};

		JSONIFIER_INLINE utf8_checker() noexcept	 = default;
		utf8_checker& operator=(utf8_checker&&)		 = delete;
		utf8_checker(utf8_checker&&)				 = delete;
		utf8_checker& operator=(const utf8_checker&) = delete;
		utf8_checker(const utf8_checker&)			 = delete;

		JSONIFIER_INLINE jsonifier_simd_int_t checkMultibyteLengths(jsonifier_simd_int_t input, jsonifier_simd_int_t prev, jsonifier_simd_int_t sc) noexcept {
			return opXor(opAnd(mustBe23Continuation(opPrev<14>(input, prev), opPrev<13>(input, prev)), gatherValue<jsonifier_simd_int_t>(static_cast<char>(0x80u))), sc);
		}

		JSONIFIER_INLINE jsonifier_simd_int_t checkSpecialCases(jsonifier_simd_int_t input, jsonifier_simd_int_t p1) noexcept {
			const jsonifier_simd_int_t loNibbleMask = gatherValue<jsonifier_simd_int_t>(static_cast<char>(0x0Fu));
			return opAnd(opAnd(opShuffle(lookupH, opAnd(opSrLi<4>(p1), loNibbleMask)), opShuffle(lookupL, opAnd(p1, loNibbleMask))),
				opShuffle(lookup2, opAnd(opSrLi<4>(input), loNibbleMask)));
		}

		JSONIFIER_INLINE jsonifier_simd_int_t mustBe23Continuation(jsonifier_simd_int_t p2, jsonifier_simd_int_t p3) noexcept {
			return opOr(opSubs(p2, gatherValue<jsonifier_simd_int_t>(static_cast<char>(0xE0u - 0x80u))),
				opSubs(p3, gatherValue<jsonifier_simd_int_t>(static_cast<char>(0xF0u - 0x80u))));
		}

		JSONIFIER_INLINE void checkChunk(jsonifier_simd_int_t input, jsonifier_simd_int_t prev) noexcept {
			error = opOr(error, checkMultibyteLengths(input, prev, checkSpecialCases(input, opPrev<15>(input, prev))));
		}

		JSONIFIER_INLINE jsonifier_simd_int_t checkIncomplete(jsonifier_simd_int_t input) noexcept {
			return opSubs(input, gatherValues<jsonifier_simd_int_t>(isIncompleteMax + (64 - simdBytesPerRegister)));
		}

		JSONIFIER_INLINE void checkStepImpl(read_buffer_ptr src) noexcept {
			simd_array_t chunks = chunk_loader<make_integer_sequence<simdRegistersPerBlock>>::impl(src);

			if (isAscii(simd::orAll<jsonifier_simd_int_t>(chunks))) {
				error		   = opOr(error, prevIncomplete);
				prevInput	   = chunks.template get<simdRegistersPerBlock - 1>();
				prevIncomplete = jsonifier_simd_int_t{};
				return;
			}

			chunk_processor<make_integer_sequence<simdRegistersPerBlock>>::impl(*this, chunks, prevInput);

			prevInput	   = chunks.template get<simdRegistersPerBlock - 1>();
			prevIncomplete = checkIncomplete(chunks.template get<simdRegistersPerBlock - 1>());
		}

		JSONIFIER_INLINE void checkStep(read_buffer_ptr srcNew) noexcept {
			step_checker_type::impl(srcNew);
		}

		JSONIFIER_INLINE bool errors() noexcept {
			return !opTest(opOr(error, prevIncomplete));
		}

		JSONIFIER_INLINE void reset() noexcept {
			prevIncomplete = jsonifier_simd_int_t{};
			prevInput	   = jsonifier_simd_int_t{};
			error		   = jsonifier_simd_int_t{};
		}

		const jsonifier_simd_int_t lookupH{ gatherValues<jsonifier_simd_int_t>(std::bit_cast<const jsonifier_simd_int_t* __restrict>(byte1HighTable)) };
		const jsonifier_simd_int_t lookup2{ gatherValues<jsonifier_simd_int_t>(std::bit_cast<const jsonifier_simd_int_t* __restrict>(byte2HighTable)) };
		const jsonifier_simd_int_t lookupL{ gatherValues<jsonifier_simd_int_t>(std::bit_cast<const jsonifier_simd_int_t* __restrict>(byte1LowTable)) };
		jsonifier_simd_int_t prevIncomplete;
		jsonifier_simd_int_t prevInput;
		jsonifier_simd_int_t error;
	};

	inline bool validateUtf8(read_buffer_ptr src, uint64_t len) noexcept {
		if (len == 0) {
			return true;
		}

		utf8_checker checker{};
		checker.reset();

		uint64_t i = 0;

		for (; i + simdBytesPerStep <= len; i += simdBytesPerStep) {
			checker.checkStep(src + i);
		}

		if (i < len) {
			alignas(simdBytesPerRegister) uint8_t tmp[simdBytesPerStep];
			std::memset(tmp, 0x41, simdBytesPerStep);
			jsonifierMemcpyUpTo<simdBytesPerStep - 1>(tmp, src + i, len - i);
			checker.checkStep(tmp);
		}

		return !checker.errors();
	}

	struct utf8_validation_state {
		JSONIFIER_INLINE utf8_validation_state() noexcept {
			reset();
		}

		JSONIFIER_INLINE void reset() noexcept {
			prevIncomplete = false;
			error		   = false;
			prevBytes[0]   = 0;
			prevBytes[1]   = 0;
			prevBytes[2]   = 0;
		}

		uint8_t prevBytes[3];
		bool prevIncomplete;
		bool error;
	};

	template<typename simd_type_new> struct utf8_register_validator {
		static constexpr uint64_t bytesProcessed = sizeof(typename simd_type_new::type);
		using simd_type							 = typename simd_type_new::type;
		using simd_type_alias					 = simd_type;

		JSONIFIER_INLINE utf8_register_validator(utf8_validation_state& stateNew) noexcept : state{ stateNew } {
			error = simd_type{};
			if (!state.prevIncomplete && ((state.prevBytes[0] | state.prevBytes[1] | state.prevBytes[2]) & 0x80u) == 0) [[likely]] {
				incompleteRegister = simd_type{};
				prevInput		   = simd_type{};
				touched			   = false;
				return;
			}
			alignas(simdBytesPerRegister) uint8_t tmp[bytesProcessed];
			tmp[bytesProcessed - 3] = state.prevBytes[0];
			tmp[bytesProcessed - 2] = state.prevBytes[1];
			tmp[bytesProcessed - 1] = state.prevBytes[2];
			incompleteRegister		= state.prevIncomplete ? simd::gatherValue<simd_type>(static_cast<char>(0x80u)) : simd_type{};
			prevInput				= simd::gatherValues<simd_type>(std::bit_cast<const simd_type* __restrict>(+tmp));
			touched					= true;
		}

		utf8_register_validator& operator=(const utf8_register_validator&) = delete;
		utf8_register_validator(const utf8_register_validator&)			   = delete;

		JSONIFIER_INLINE void checkRegister(simd_type input) noexcept {
			if (simd::isAscii(input)) {
				error			   = simd::opOr(error, incompleteRegister);
				prevInput		   = input;
				incompleteRegister = simd_type{};
				return;
			}
			touched			   = true;
			const simd_type sc = checkSpecialCases(input, simd::opPrev<15>(input, prevInput));
			error = simd::opOr(error, simd::opXor(simd::opAnd(mustBe23Continuation(simd::opPrev<14>(input, prevInput), simd::opPrev<13>(input, prevInput)), maskNibble01()), sc));
			prevInput		   = input;
			incompleteRegister = simd::opSubs(input, incompleteMax());
		}

		JSONIFIER_INLINE simd_type checkSpecialCases(simd_type input, simd_type p1) noexcept {
			return simd::opAnd(
				simd::opAnd(simd::opShuffle(lookupH(), simd::opAnd(simd::opSrLi<4>(p1), loNibbleMask())), simd::opShuffle(lookupL(), simd::opAnd(p1, loNibbleMask()))),
				simd::opShuffle(lookup2(), simd::opAnd(simd::opSrLi<4>(input), loNibbleMask())));
		}

		JSONIFIER_INLINE void flush() noexcept {
			if (!touched) {
				return;
			}
			alignas(simdBytesPerRegister) uint8_t tmp[bytesProcessed];
			simd::store(prevInput, tmp);
			state.prevBytes[0]	 = tmp[bytesProcessed - 3];
			state.prevBytes[1]	 = tmp[bytesProcessed - 2];
			state.prevBytes[2]	 = tmp[bytesProcessed - 1];
			state.prevIncomplete = tmp[bytesProcessed - 1] >= 0xC0u || tmp[bytesProcessed - 2] >= 0xE0u || tmp[bytesProcessed - 3] >= 0xF0u;
			if (!simd::opTest(error)) {
				state.error = true;
			}
		}

		JSONIFIER_INLINE simd_type mustBe23Continuation(simd_type p2, simd_type p3) noexcept {
			return simd::opOr(simd::opSubs(p2, continuationMask01()), simd::opSubs(p3, continuationMask02()));
		}

		JSONIFIER_INLINE static simd_type lookupH() noexcept {
			return simd::gatherValues<simd_type>(std::bit_cast<const simd_type* __restrict>(byte1HighTable));
		}

		JSONIFIER_INLINE static simd_type lookup2() noexcept {
			return simd::gatherValues<simd_type>(std::bit_cast<const simd_type* __restrict>(byte2HighTable));
		}

		JSONIFIER_INLINE static simd_type lookupL() noexcept {
			return simd::gatherValues<simd_type>(std::bit_cast<const simd_type* __restrict>(byte1LowTable));
		}

		JSONIFIER_INLINE void checkPartial(const void* __restrict src, uint64_t count) noexcept {
			if (count == 0) {
				return;
			}
			alignas(simdBytesPerRegister) uint8_t tmp[bytesProcessed];
			std::memset(tmp, 32, bytesProcessed);
			jsonifierMemcpyUpTo<bytesProcessed - 1>(tmp, src, count);
			checkRegister(simd::gatherValues<simd_type>(std::bit_cast<const simd_type* __restrict>(+tmp)));
		}

		JSONIFIER_INLINE static simd_type incompleteMax() noexcept {
			return simd::gatherValues<simd_type>(isIncompleteMax + (64 - bytesProcessed));
		}

		JSONIFIER_INLINE bool errors() noexcept {
			return state.error || !simd::opTest(simd::opOr(error, incompleteRegister));
		}

		JSONIFIER_INLINE static simd_type continuationMask01() noexcept {
			return simd::gatherValue<simd_type>(static_cast<char>(0xE0u - 0x80u));
		}

		JSONIFIER_INLINE static simd_type continuationMask02() noexcept {
			return simd::gatherValue<simd_type>(static_cast<char>(0xF0u - 0x80u));
		}

		JSONIFIER_INLINE static simd_type maskNibble01() noexcept {
			return simd::gatherValue<simd_type>(static_cast<char>(0x80u));
		}

		JSONIFIER_INLINE static simd_type loNibbleMask() noexcept {
			return simd::gatherValue<simd_type>(static_cast<char>(0x0Fu));
		}

		JSONIFIER_INLINE bool hardErrors() noexcept {
			return state.error || !simd::opTest(error);
		}

		JSONIFIER_INLINE void reset() noexcept {
			state.reset();
			prevInput		   = simd_type{};
			incompleteRegister = simd_type{};
			error			   = simd_type{};
			touched			   = false;
		}

		simd_type incompleteRegister;
		simd_type prevInput;
		simd_type error;
		utf8_validation_state& state;
		bool touched;
	};

}

namespace jsonifier {

	inline bool validateUtf8(internal::read_buffer_ptr src, uint64_t len) noexcept {
		return internal::validateUtf8(src, len);
	}

}
