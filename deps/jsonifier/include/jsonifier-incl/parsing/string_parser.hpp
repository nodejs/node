/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/parsing/string_parser.hpp
 */
#pragma once

#include <jsonifier-incl/parsing/generic_tables.hpp>

namespace jsonifier::internal {

	// Sampled from Dr. Lemire's library, simdjson: https://github.com/simdjson/simdjson
	struct string_parser {
		using scan_result = typename string_scanner<parse_options{}>::scan_result;

		struct parse_state {
			internal::utf8_validation_state validationState{};
			scan_result result{};
		};

		struct parse_step {
			template<uint64_t index, typename basic_iterator01, typename basic_iterator02> JSONIFIER_INLINE static basic_iterator01 impl(basic_iterator01& current,
				const basic_iterator01 end, basic_iterator02& out, const basic_iterator01 stringStart, const basic_iterator02 outStart, parse_state& state) noexcept {
				using simd_list_local					 = type_list_element_t<index, simd::avx_integer_list>;
				using integer_type						 = typename simd_list_local::integer_type;
				static constexpr auto avx_type			 = simd_list_local::type::avx_type;
				using simd_type_local					 = typename simd_type_wrapper<avx_type>::type;
				static constexpr uint64_t bytesProcessed = simd_list_local::bytesProcessed;
				internal::utf8_register_validator<simd_type_wrapper<avx_type>> validator{ state.validationState };
				const simd_type_local backslashes = simd::gatherValue<simd_type_local>('\\');
				const simd_type_local quotes	  = simd::gatherValue<simd_type_local>('"');
				const simd_type_local controls	  = simd::gatherValue<simd_type_local>(static_cast<char>(32));
				while (static_cast<uint64_t>(end - current) >= bytesProcessed) {
					const simd_type_local values = simd::gatherValuesU<simd_type_local>(current);
					simd::storeU(values, out);
					const integer_type stops = static_cast<integer_type>(
						simd::opBitMask(simd::opOr(simd::opOr(simd::opCmpEqRaw(values, backslashes), simd::opCmpEqRaw(values, quotes)), simd::opCmpLtRaw(values, controls))));
					if (stops == static_cast<integer_type>(0)) {
						validator.checkRegister(values);
						current += bytesProcessed;
						out += bytesProcessed;
						continue;
					}
					if (!resolveStop(validator, static_cast<uint64_t>(simd::countrZeroUnsafe(stops)), current, end, out, stringStart, outStart, state)) {
						return nullptr;
					}
				}
				validator.flush();
				if constexpr (index == listSize - 1) {
					while (current < end) {
						const uint64_t remaining = static_cast<uint64_t>(end - current);
						alignas(simdBytesPerRegister) uint8_t padded[bytesProcessed];
						std::memset(padded, 32, bytesProcessed);
						jsonifierMemcpyUpTo<bytesProcessed - 1>(padded, current, remaining);
						const simd_type_local values = simd::gatherValues<simd_type_local>(std::bit_cast<const simd_type_local* __restrict>(+padded));
						const integer_type stops	 = static_cast<integer_type>(
							simd::opBitMask(simd::opOr(simd::opOr(simd::opCmpEqRaw(values, backslashes), simd::opCmpEqRaw(values, quotes)), simd::opCmpLtRaw(values, controls))));
						if (stops == static_cast<integer_type>(0)) {
							return nullptr;
						}
						const uint64_t offset = static_cast<uint64_t>(simd::countrZeroUnsafe(stops));
						jsonifierMemcpyUpTo<bytesProcessed - 1>(out, current, offset);
						if (!resolveStop(validator, offset, current, end, out, stringStart, outStart, state)) {
							return nullptr;
						}
					}
					return nullptr;
				} else {
					return current;
				}
			}
		};

		template<typename validator_type, typename basic_iterator01, typename basic_iterator02> JSONIFIER_INLINE static bool resolveStop(validator_type& validator, uint64_t offset,
			basic_iterator01& current, const basic_iterator01 end, basic_iterator02& out, const basic_iterator01 stringStart, const basic_iterator02 outStart,
			parse_state& state) noexcept {
			const uint8_t foundChar = static_cast<uint8_t>(current[offset]);
			if (foundChar < 32) [[unlikely]] {
				return false;
			}
			validator.checkPartial(current, offset);
			current += offset;
			out += offset;
			if (validator.errors()) [[unlikely]] {
				return false;
			}
			if (foundChar == uint_c_v<'"'>) {
				state.result = { static_cast<uint64_t>(out - outStart), static_cast<uint64_t>(current - stringStart) };
				return false;
			}
			if (!parseEscape(current, end, out)) [[unlikely]] {
				return false;
			}
			validator.reset();
			return true;
		}

		template<typename basic_iterator01, typename basic_iterator02>
		JSONIFIER_INLINE static scan_result impl(basic_iterator01 current, const basic_iterator01 end, basic_iterator02 out) noexcept {
			const auto stringStart = current;
			const auto outStart	   = out;
			parse_state state{};
			string_parse_executor<parse_step, make_ascending_range<startIndex, listSize>>::impl(current, end, out, stringStart, outStart, state);
			return state.result;
		}

		template<typename basic_iterator01, typename basic_iterator02>
		JSONIFIER_INLINE static bool parseEscape(basic_iterator01& current, const basic_iterator01 end, basic_iterator02& out) noexcept {
			if (end - current < 2) [[unlikely]] {
				return false;
			}
			if (current[1] == 'u') {
				do {
					if (!handleUnicodeCodePoint(current, out, end)) [[unlikely]] {
						return false;
					}
				} while (end - current >= 2 && current[0] == '\\' && current[1] == 'u');
				return true;
			}
			const uint8_t decoded = static_cast<uint8_t>(escapeMap[static_cast<uint8_t>(current[1])]);
			if (decoded == 0u) [[unlikely]] {
				return false;
			}
			*out = static_cast<remove_cvref_t<decltype(*out)>>(decoded);
			++out;
			current += 2;
			return true;
		}
	};

	struct string_parse_context {
		string_parse_context() noexcept = delete;

		JSONIFIER_INLINE string_parse_context(read_buffer_ptr sourceNew, read_buffer_ptr sourceEndNew, uint64_t& outLengthNew) noexcept
			: sourceEnd{ sourceEndNew }, source{ sourceNew }, outLength{ outLengthNew } {
		}

		string_parse_context& operator=(string_parse_context&&) noexcept	  = delete;
		string_parse_context(string_parse_context&&) noexcept				  = delete;
		string_parse_context& operator=(const string_parse_context&) noexcept = delete;
		string_parse_context(const string_parse_context&) noexcept			  = delete;

		JSONIFIER_INLINE uint64_t operator()(char* __restrict ptrNew, uint64_t) const noexcept {
			outLength = string_parser::impl(source, sourceEnd, static_cast<char*>(ptrNew)).outLength;
			return outLength == std::numeric_limits<uint64_t>::max() ? uint64_t{} : outLength;
		}

		read_buffer_ptr sourceEnd{};
		read_buffer_ptr source{};
		uint64_t& outLength;
	};

}
