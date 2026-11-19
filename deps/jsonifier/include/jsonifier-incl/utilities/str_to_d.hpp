/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/utilities/str_to_d.hpp
 */
#pragma once

#include <jsonifier-incl/utilities/fast_float.hpp>
#include <jsonifier-incl/utilities/utility.hpp>

namespace jsonifier::internal {

	consteval array<bool, 256> generateExpTable() noexcept {
		array<bool, 256> table{};
		table[uint_c_v<'E'>] = true;
		table[uint_c_v<'e'>] = true;
		return table;
	}

	template<typename = void> alignas(64) inline constexpr array<bool, 256> expTableRaw = generateExpTable();

	inline constexpr const bool* __restrict expTable{ expTableRaw<>.data() };

	consteval array<bool, 256> generateExpFracTable() noexcept {
		array<bool, 256> table{};
		table[uint_c_v<'.'>] = true;
		table[uint_c_v<'E'>] = true;
		table[uint_c_v<'e'>] = true;
		return table;
	}

	template<typename = void> alignas(64) inline constexpr array<bool, 256> expFracTableRaw = generateExpFracTable();

	inline constexpr const bool* __restrict expFracTable{ expFracTableRaw<>.data() };

	enum class number_shapes : uint8_t {
		general,
		short_fraction,
		long_integer,
	};

	struct number_shape_detector {
		static constexpr uint64_t longIntegerThreshold{ 6 };
		static constexpr uint64_t shortFractionLimit{ 8 };

		JSONIFIER_INLINE static number_shapes impl(read_buffer_ptr iter, read_buffer_ptr end) noexcept {
			while (iter < end && ( *iter == uint_c_v<' '> || *iter == uint_c_v<'\t'> || *iter == uint_c_v<'\n'> || *iter == uint_c_v<'\r'> )) {
				++iter;
			}
			if (iter < end && *iter == uint_c_v<'-'>) {
				++iter;
			}
			read_buffer_ptr const integerStart = iter;
			while (iter < end && isDigit(*iter)) {
				++iter;
			}
			if (static_cast<uint64_t>(iter - integerStart) >= longIntegerThreshold) {
				return number_shapes::long_integer;
			}
			if (iter >= end || *iter != uint_c_v<'.'>) {
				return number_shapes::general;
			}
			++iter;
			read_buffer_ptr const fractionStart = iter;
			while (iter < end && isDigit(*iter)) {
				++iter;
			}
			uint64_t const fractionDigits = static_cast<uint64_t>(iter - fractionStart);
			return (fractionDigits > 0 && fractionDigits < shortFractionLimit) ? number_shapes::short_fraction : number_shapes::general;
		}
	};

	struct parsed_number {
		read_buffer_ptr lastMatch{};
		span<uint8_t> fraction{};
		span<uint8_t> integer{};
		uint64_t mantissa{};
		int64_t exponent{};
		bool tooManyDigits{};
		bool negative{};
		bool valid{};
	};

	template<number_shapes shape> JSONIFIER_INLINE parsed_number parseNumberString(read_buffer_ptr iter, read_buffer_ptr end) noexcept {
		parsed_number answer{};
		answer.negative = (*iter == uint_c_v<'-'>);
		if (answer.negative) {
			++iter;
			if (iter == end || !isDigit(*iter)) [[unlikely]] {
				return answer;
			}
		}
		read_buffer_ptr const startDigits = iter;

		uint64_t mantissa{};
		if ((iter != end) && isDigit(*iter)) {
			mantissa = static_cast<uint64_t>(*iter - uint_c_v<'0'>);
			++iter;
			if constexpr (shape == number_shapes::long_integer) {
				loopParseIfEightDigits(iter, end, mantissa);
				while ((iter != end) && isDigit(*iter)) {
					mantissa = 10 * mantissa + static_cast<uint64_t>(*iter - uint_c_v<'0'>);
					++iter;
				}
			} else {
				if ((iter != end) && isDigit(*iter)) {
					mantissa = 10 * mantissa + static_cast<uint64_t>(*iter - uint_c_v<'0'>);
					++iter;
					if ((iter != end) && isDigit(*iter)) {
						mantissa = 10 * mantissa + static_cast<uint64_t>(*iter - uint_c_v<'0'>);
						++iter;
						if ((iter != end) && isDigit(*iter)) {
							mantissa = 10 * mantissa + static_cast<uint64_t>(*iter - uint_c_v<'0'>);
							++iter;
							if ((iter != end) && isDigit(*iter)) {
								mantissa = 10 * mantissa + static_cast<uint64_t>(*iter - uint_c_v<'0'>);
								++iter;
								loopParseIfEightDigits(iter, end, mantissa);
								while ((iter != end) && isDigit(*iter)) {
									mantissa = 10 * mantissa + static_cast<uint64_t>(*iter - uint_c_v<'0'>);
									++iter;
								}
							}
						}
					}
				}
			}
		}
		read_buffer_ptr const endOfIntegerPart = iter;
		int64_t digitCount					   = static_cast<int64_t>(endOfIntegerPart - startDigits);
		answer.integer						   = span<uint8_t>{ startDigits, endOfIntegerPart };
		if (digitCount == 0 || (startDigits[0] == uint_c_v<'0'> && digitCount > 1)) [[unlikely]] {
			return answer;
		}

		int64_t exponent{};
		bool const hasDecimalPoint = (iter != end) && (*iter == uint_c_v<'.'>);
		if (hasDecimalPoint) {
			++iter;
			read_buffer_ptr const before = iter;
			if constexpr (shape != number_shapes::short_fraction) {
				loopParseIfEightDigits(iter, end, mantissa);
			}
			while ((iter != end) && isDigit(*iter)) {
				uint8_t digit = static_cast<uint8_t>(*iter - uint_c_v<'0'>);
				++iter;
				mantissa = mantissa * 10 + digit;
			}
			exponent		= before - iter;
			answer.fraction = span<uint8_t>{ before, iter };
			digitCount -= exponent;
			if (exponent == 0) [[unlikely]] {
				return answer;
			}
		}

		int64_t expNumber{};
		if ((iter != end) && (( *iter == uint_c_v<'e'> ) || ( *iter == uint_c_v<'E'> ))) {
			++iter;
			bool negExp = false;
			if ((iter != end) && ( *iter == uint_c_v<'-'> )) {
				negExp = true;
				++iter;
			} else if ((iter != end) && ( *iter == uint_c_v<'+'> )) {
				++iter;
			}
			if ((iter == end) || !isDigit(*iter)) [[unlikely]] {
				return answer;
			}
			while ((iter != end) && isDigit(*iter)) {
				uint8_t digit = static_cast<uint8_t>(*iter - uint_c_v<'0'>);
				if (expNumber < 0x10000000) {
					expNumber = 10 * expNumber + digit;
				}
				++iter;
			}
			if (negExp) {
				expNumber = -expNumber;
			}
			exponent += expNumber;
		}
		answer.lastMatch = iter;
		answer.valid	 = true;

		if (digitCount > 19) [[unlikely]] {
			read_buffer_ptr start = startDigits;
			while ((start != end) && ( *start == uint_c_v<'0'> || *start == uint_c_v<'.'> )) {
				if (*start == uint_c_v<'0'>) {
					--digitCount;
				}
				++start;
			}
			if (digitCount > 19) {
				answer.tooManyDigits = true;
				static constexpr uint64_t minNineteenDigitInteger{ 1000000000000000000 };
				mantissa = 0;
				iter	 = answer.integer.ptr;
				while ((mantissa < minNineteenDigitInteger) && (iter != answer.integer.end)) {
					mantissa = mantissa * 10 + static_cast<uint64_t>(*iter - uint_c_v<'0'>);
					++iter;
				}
				if (mantissa >= minNineteenDigitInteger) {
					exponent = endOfIntegerPart - iter + expNumber;
				} else {
					iter = answer.fraction.ptr;
					while ((mantissa < minNineteenDigitInteger) && (iter != answer.fraction.end)) {
						mantissa = mantissa * 10 + static_cast<uint64_t>(*iter - uint_c_v<'0'>);
						++iter;
					}
					exponent = answer.fraction.ptr - iter + expNumber;
				}
			}
		}
		answer.exponent = exponent;
		answer.mantissa = mantissa;
		return answer;
	}

	template<float_types value_type> struct float_parser {
		template<number_shapes shape = number_shapes::general>
		JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> parseFloat(value_type& value, read_buffer_ptr iter, read_buffer_ptr end = nullptr) noexcept {
			if (iter >= end) [[unlikely]] {
				return nullptr;
			}
			parsed_number pns = parseNumberString<shape>(iter, end);
			if (!pns.valid) [[unlikely]] {
				return nullptr;
			}
			if (!pns.tooManyDigits && clingerFastPath(pns.mantissa, pns.exponent, pns.negative, value)) [[likely]] {
				return pns.lastMatch;
			}
			adjusted_mantissa am = computeFloat<binary_format<value_type>>(pns.exponent, pns.mantissa);
			if (pns.tooManyDigits && am.power2 >= 0) [[unlikely]] {
				if (am != computeFloat<binary_format<value_type>>(pns.exponent, pns.mantissa + 1)) {
					am = computeError<binary_format<value_type>>(pns.exponent, pns.mantissa);
				}
			}
			if (am.power2 < 0) [[unlikely]] {
				am = digitComp<value_type>(pns.integer, pns.fraction, pns.mantissa, pns.exponent, am);
			}
			if (am.power2 == binary_format<value_type>::infinitePower) [[unlikely]] {
				return nullptr;
			}
			toFloat(pns.negative, am, value);
			return pns.lastMatch;
		}

		JSONIFIER_INLINE static bool clingerFastPath(uint64_t mantissa, int64_t exponent, bool negative, value_type& value) noexcept {
			if (binary_format<value_type>::minExponentFastPath <= exponent && exponent <= binary_format<value_type>::maxExponentFastPath &&
				mantissa <= binary_format<value_type>::maxMantissaFastPathValue) {
				if (rounds_to_nearest::roundsToNearest) {
					value = static_cast<value_type>(mantissa);
					if (exponent < 0) {
						value = value / binary_format<value_type>::exactPowerOfTen(-exponent);
					} else {
						value = value * binary_format<value_type>::exactPowerOfTen(exponent);
					}
					if (negative) {
						value = -value;
					}
					return true;
				} else if (exponent >= 0 && mantissa <= binary_format<value_type>::maxMantissaFastPath(exponent)) {
#if JSONIFIER_COMPILER_CLANG
					if (mantissa == 0) {
						value = negative ? static_cast<value_type>(-0.) : static_cast<value_type>(0.);
						return true;
					}
#endif
					value = static_cast<value_type>(mantissa) * binary_format<value_type>::exactPowerOfTen(exponent);
					if (negative) {
						value = -value;
					}
					return true;
				}
			}
			return false;
		}
	};

}
