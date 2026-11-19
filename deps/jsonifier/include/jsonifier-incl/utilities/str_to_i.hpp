/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/utilities/str_to_i.hpp
 */
#pragma once

#include <jsonifier-incl/containers/allocator.hpp>
#include <jsonifier-incl/utilities/fast_float.hpp>
#include <jsonifier-incl/utilities/str_to_d.hpp>

namespace jsonifier::internal {

#if JSONIFIER_COMPILER_MSVC

	template<bool negative> inline constexpr uint64_t compValAddition{ [] {
		if constexpr (negative) {
			return 1ULL;
		} else {
			return 0ULL;
		}
	}() };

	template<typename v_type, bool negative> consteval array<std::make_unsigned_t<v_type>, 256ULL> genRawCompVals() noexcept {
		constexpr auto maxValue{ static_cast<std::make_unsigned_t<v_type>>(std::numeric_limits<remove_cvref_t<v_type>>::max()) + compValAddition<negative> };
		array<std::make_unsigned_t<v_type>, 256ULL> returnValues{};
		returnValues[uint_c_v<'0'>] = (maxValue - 0) / 10;
		returnValues[uint_c_v<'1'>] = (maxValue - 1) / 10;
		returnValues[uint_c_v<'2'>] = (maxValue - 2) / 10;
		returnValues[uint_c_v<'3'>] = (maxValue - 3) / 10;
		returnValues[uint_c_v<'4'>] = (maxValue - 4) / 10;
		returnValues[uint_c_v<'5'>] = (maxValue - 5) / 10;
		returnValues[uint_c_v<'6'>] = (maxValue - 6) / 10;
		returnValues[uint_c_v<'7'>] = (maxValue - 7) / 10;
		returnValues[uint_c_v<'8'>] = (maxValue - 8) / 10;
		returnValues[uint_c_v<'9'>] = (maxValue - 9) / 10;
		return returnValues;
	}

	template<typename v_type, bool negative> alignas(64) inline constexpr auto compValsRaw{ genRawCompVals<v_type, negative>() };

	template<typename v_type, bool negative> inline constexpr const std::make_unsigned_t<v_type>* __restrict compVals{ compValsRaw<v_type, negative>.data() };

	JSONIFIER_INLINE uint8_t peekByte(const uint8_t* iter, const uint8_t* end) noexcept {
		return iter < end ? *iter : uint8_t{ 0 };
	}

	template<bool checked> JSONIFIER_INLINE uint8_t loadByte(const uint8_t* iter, const uint8_t* end) noexcept {
		if constexpr (checked) {
			return peekByte(iter, end);
		} else {
			return *iter;
		}
	}

	inline constexpr ptrdiff_t uncheckedIntegerSpan{ 21 };

	inline constexpr int64_t maxFractionDigits{ 18 };

	inline constexpr int64_t maxUnsignedFractionDigits{ 19 };

	inline constexpr int64_t maxExponentValue{ 20 };

	template<typename value_type, size_t count> consteval array<value_type, count> generatePowerOfTen() noexcept {
		array<value_type, count> table{};
		value_type val = 1;
		for (size_t i = 0; i < count; ++i) {
			table[i] = val;
			if (i + 1 < count) {
				val *= 10;
			}
		}
		return table;
	}

	template<typename> struct powers_of_ten;

	template<int_types value_type> struct powers_of_ten<value_type> {
		alignas(64) static constexpr array<int64_t, 19> valueRaw{ generatePowerOfTen<int64_t, 19>() };

		static constexpr const int64_t* __restrict value{ valueRaw.data() };
	};

	template<uint_types value_type> struct powers_of_ten<value_type> {
		alignas(64) static constexpr array<uint64_t, 20> valueRaw{ generatePowerOfTen<uint64_t, 20>() };

		static constexpr const uint64_t* __restrict value{ valueRaw.data() };
	};

	template<typename value_type> struct integer_parser;

	template<integer_types value_type> JSONIFIER_INLINE static value_type mul128Generic(value_type ab, value_type cd, value_type& hi) noexcept {
		value_type aHigh = ab >> 32;
		value_type aLow	 = ab & 0xFFFFFFFF;
		value_type bHigh = cd >> 32;
		value_type bLow	 = cd & 0xFFFFFFFF;
		value_type loLo	 = aLow * bLow;
		value_type loHi	 = aLow * bHigh;
		value_type hiLo	 = aHigh * bLow;
		value_type hiHi	 = aHigh * bHigh;
		value_type cross = (loLo >> 32) + (loHi & 0xFFFFFFFF) + (hiLo & 0xFFFFFFFF);
		value_type lo	 = (cross << 32) | (loLo & 0xFFFFFFFF);
		hi				 = hiHi + (loHi >> 32) + (hiLo >> 32) + (cross >> 32);
		return lo;
	}

	template<int_types value_type> JSONIFIER_INLINE static bool multiply(value_type& value, value_type expValue) noexcept {
	#if JSONIFIER_HAS_INT128
		const __int128_t res = static_cast<__int128_t>(value) * static_cast<__int128_t>(expValue);
		value				 = static_cast<value_type>(res);
		return res <= std::numeric_limits<value_type>::max();
	#elif JSONIFIER_COMPILER_MSVC && JSONIFIER_ARCH_X64
		value_type values;
		value = _mul128(value, expValue, &values);
		return values == 0;
	#elif JSONIFIER_COMPILER_CLANG || JSONIFIER_COMPILER_GCC
		return !__builtin_mul_overflow(value, expValue, &value);
	#else
		value_type values;
		value = mul128Generic(value, expValue, values);
		return values == 0;
	#endif
	}

	template<uint_types value_type> JSONIFIER_INLINE static bool multiply(value_type& value, value_type expValue) noexcept {
	#if JSONIFIER_HAS_INT128
		const __uint128_t res = static_cast<__uint128_t>(value) * static_cast<__uint128_t>(expValue);
		value				  = static_cast<value_type>(res);
		return res <= std::numeric_limits<value_type>::max();
	#elif JSONIFIER_COMPILER_MSVC && JSONIFIER_ARCH_X64
		value_type values;
		value = _umul128(value, expValue, &values);
		return values == 0;
	#elif JSONIFIER_COMPILER_CLANG || JSONIFIER_COMPILER_GCC
		return !__builtin_mul_overflow(value, expValue, &value);
	#else
		value_type values;
		value = umul128Generic(value, expValue, values);
		return values == 0;
	#endif
	}

	template<int_types value_type> JSONIFIER_INLINE static bool divide(value_type& value, value_type expValue) noexcept {
	#if JSONIFIER_HAS_INT128
		const __int128_t dividend = static_cast<__int128_t>(value);
		value					  = static_cast<value_type>(dividend / static_cast<__int128_t>(expValue));
		return (dividend % static_cast<__int128_t>(expValue)) == 0;
	#elif JSONIFIER_COMPILER_MSVC && JSONIFIER_ARCH_X64
		value_type values;
		value = _div128(0, value, expValue, &values);
		return values == 0;
	#else
		value_type values;
		values = value % expValue;
		value  = value / expValue;
		return values == 0;
	#endif
	}

	template<uint_types value_type> JSONIFIER_INLINE static bool divide(value_type& value, value_type expValue) noexcept {
	#if JSONIFIER_HAS_INT128
		const __uint128_t dividend = static_cast<__uint128_t>(value);
		value					   = static_cast<value_type>(dividend / static_cast<__uint128_t>(expValue));
		return (dividend % static_cast<__uint128_t>(expValue)) == 0;
	#elif JSONIFIER_COMPILER_MSVC && JSONIFIER_ARCH_X64
		value_type values;
		value = _udiv128(0, value, expValue, &values);
		return values == 0;
	#else
		value_type values;
		values = value % expValue;
		value  = value / expValue;
		return values == 0;
	#endif
	}

	template<integer_types value_type> struct integer_parser<value_type> {
		inline constexpr integer_parser() noexcept = default;

		template<bool negative, bool checked> JSONIFIER_INLINE static const uint8_t* parseInteger(value_type& value, const uint8_t* iter, const uint8_t* end) noexcept {
			using v_type_local = std::make_unsigned_t<value_type>;
			uint8_t numTmp{ loadByte<checked>(iter, end) };
			if (isDigit(numTmp)) [[likely]] {
				value = static_cast<value_type>(numTmp - uint_c_v<'0'>);
				++iter;
				numTmp = loadByte<checked>(iter, end);
			} else [[unlikely]] {
				return nullptr;
			}

			if (isDigit(numTmp)) [[likely]] {
				value = static_cast<value_type>(static_cast<v_type_local>(value) * 10 + ( numTmp - uint_c_v<'0'> ));
				++iter;
				numTmp = loadByte<checked>(iter, end);
			} else [[unlikely]] {
				if (!expFracTable[numTmp]) [[likely]] {
					if constexpr (negative) {
						value *= -1;
					}
					return iter;
				}
				if constexpr (negative) {
					return (iter = finishParse(value, iter, end), value *= -1, iter);
				} else {
					return finishParse(value, iter, end);
				}
			}

			if (iter[-2] == uint_c_v<'0'>) [[unlikely]] {
				return nullptr;
			}

			if (isDigit(numTmp)) [[likely]] {
				value = static_cast<value_type>(static_cast<v_type_local>(value) * 10 + ( numTmp - uint_c_v<'0'> ));
				++iter;
				numTmp = loadByte<checked>(iter, end);
			} else [[unlikely]] {
				if (!expFracTable[numTmp]) [[likely]] {
					if constexpr (negative) {
						value *= -1;
					}
					return iter;
				}
				if constexpr (negative) {
					return (iter = finishParse(value, iter, end), value *= -1, iter);
				} else {
					return finishParse(value, iter, end);
				}
			}

			if (isDigit(numTmp)) [[likely]] {
				value = static_cast<value_type>(static_cast<v_type_local>(value) * 10 + ( numTmp - uint_c_v<'0'> ));
				++iter;
				numTmp = loadByte<checked>(iter, end);
			} else [[unlikely]] {
				if (!expFracTable[numTmp]) [[likely]] {
					if constexpr (negative) {
						value *= -1;
					}
					return iter;
				}
				if constexpr (negative) {
					return (iter = finishParse(value, iter, end), value *= -1, iter);
				} else {
					return finishParse(value, iter, end);
				}
			}

			if (isDigit(numTmp)) [[likely]] {
				value = static_cast<value_type>(static_cast<v_type_local>(value) * 10 + ( numTmp - uint_c_v<'0'> ));
				++iter;
				numTmp = loadByte<checked>(iter, end);
			} else [[unlikely]] {
				if (!expFracTable[numTmp]) [[likely]] {
					if constexpr (negative) {
						value *= -1;
					}
					return iter;
				}
				if constexpr (negative) {
					return (iter = finishParse(value, iter, end), value *= -1, iter);
				} else {
					return finishParse(value, iter, end);
				}
			}

			if (isDigit(numTmp)) [[likely]] {
				value = static_cast<value_type>(static_cast<v_type_local>(value) * 10 + ( numTmp - uint_c_v<'0'> ));
				++iter;
				numTmp = loadByte<checked>(iter, end);
			} else [[unlikely]] {
				if (!expFracTable[numTmp]) [[likely]] {
					if constexpr (negative) {
						value *= -1;
					}
					return iter;
				}
				if constexpr (negative) {
					return (iter = finishParse(value, iter, end), value *= -1, iter);
				} else {
					return finishParse(value, iter, end);
				}
			}

			if (isDigit(numTmp)) [[likely]] {
				value = static_cast<value_type>(static_cast<v_type_local>(value) * 10 + ( numTmp - uint_c_v<'0'> ));
				++iter;
				numTmp = loadByte<checked>(iter, end);
			} else [[unlikely]] {
				if (!expFracTable[numTmp]) [[likely]] {
					if constexpr (negative) {
						value *= -1;
					}
					return iter;
				}
				if constexpr (negative) {
					return (iter = finishParse(value, iter, end), value *= -1, iter);
				} else {
					return finishParse(value, iter, end);
				}
			}

			if (isDigit(numTmp)) [[likely]] {
				value = static_cast<value_type>(static_cast<v_type_local>(value) * 10 + ( numTmp - uint_c_v<'0'> ));
				++iter;
				numTmp = loadByte<checked>(iter, end);
			} else [[unlikely]] {
				if (!expFracTable[numTmp]) [[likely]] {
					if constexpr (negative) {
						value *= -1;
					}
					return iter;
				}
				if constexpr (negative) {
					return (iter = finishParse(value, iter, end), value *= -1, iter);
				} else {
					return finishParse(value, iter, end);
				}
			}

			if (isDigit(numTmp)) [[likely]] {
				value = static_cast<value_type>(static_cast<v_type_local>(value) * 10 + ( numTmp - uint_c_v<'0'> ));
				++iter;
				numTmp = loadByte<checked>(iter, end);
			} else [[unlikely]] {
				if (!expFracTable[numTmp]) [[likely]] {
					if constexpr (negative) {
						value *= -1;
					}
					return iter;
				}
				if constexpr (negative) {
					return (iter = finishParse(value, iter, end), value *= -1, iter);
				} else {
					return finishParse(value, iter, end);
				}
			}

			if (isDigit(numTmp)) [[likely]] {
				value = static_cast<value_type>(static_cast<v_type_local>(value) * 10 + ( numTmp - uint_c_v<'0'> ));
				++iter;
				numTmp = loadByte<checked>(iter, end);
			} else [[unlikely]] {
				if (!expFracTable[numTmp]) [[likely]] {
					if constexpr (negative) {
						value *= -1;
					}
					return iter;
				}
				if constexpr (negative) {
					return (iter = finishParse(value, iter, end), value *= -1, iter);
				} else {
					return finishParse(value, iter, end);
				}
			}

			if (isDigit(numTmp)) [[likely]] {
				value = static_cast<value_type>(static_cast<v_type_local>(value) * 10 + ( numTmp - uint_c_v<'0'> ));
				++iter;
				numTmp = loadByte<checked>(iter, end);
			} else [[unlikely]] {
				if (!expFracTable[numTmp]) [[likely]] {
					if constexpr (negative) {
						value *= -1;
					}
					return iter;
				}
				if constexpr (negative) {
					return (iter = finishParse(value, iter, end), value *= -1, iter);
				} else {
					return finishParse(value, iter, end);
				}
			}

			if (isDigit(numTmp)) [[likely]] {
				value = static_cast<value_type>(static_cast<v_type_local>(value) * 10 + ( numTmp - uint_c_v<'0'> ));
				++iter;
				numTmp = loadByte<checked>(iter, end);
			} else [[unlikely]] {
				if (!expFracTable[numTmp]) [[likely]] {
					if constexpr (negative) {
						value *= -1;
					}
					return iter;
				}
				if constexpr (negative) {
					return (iter = finishParse(value, iter, end), value *= -1, iter);
				} else {
					return finishParse(value, iter, end);
				}
			}

			if (isDigit(numTmp)) [[likely]] {
				value = static_cast<value_type>(static_cast<v_type_local>(value) * 10 + ( numTmp - uint_c_v<'0'> ));
				++iter;
				numTmp = loadByte<checked>(iter, end);
			} else [[unlikely]] {
				if (!expFracTable[numTmp]) [[likely]] {
					if constexpr (negative) {
						value *= -1;
					}
					return iter;
				}
				if constexpr (negative) {
					return (iter = finishParse(value, iter, end), value *= -1, iter);
				} else {
					return finishParse(value, iter, end);
				}
			}

			if (isDigit(numTmp)) [[likely]] {
				value = static_cast<value_type>(static_cast<v_type_local>(value) * 10 + ( numTmp - uint_c_v<'0'> ));
				++iter;
				numTmp = loadByte<checked>(iter, end);
			} else [[unlikely]] {
				if (!expFracTable[numTmp]) [[likely]] {
					if constexpr (negative) {
						value *= -1;
					}
					return iter;
				}
				if constexpr (negative) {
					return (iter = finishParse(value, iter, end), value *= -1, iter);
				} else {
					return finishParse(value, iter, end);
				}
			}

			if (isDigit(numTmp)) [[likely]] {
				value = static_cast<value_type>(static_cast<v_type_local>(value) * 10 + ( numTmp - uint_c_v<'0'> ));
				++iter;
				numTmp = loadByte<checked>(iter, end);
			} else [[unlikely]] {
				if (!expFracTable[numTmp]) [[likely]] {
					if constexpr (negative) {
						value *= -1;
					}
					return iter;
				}
				if constexpr (negative) {
					return (iter = finishParse(value, iter, end), value *= -1, iter);
				} else {
					return finishParse(value, iter, end);
				}
			}

			if (isDigit(numTmp)) [[likely]] {
				value = static_cast<value_type>(static_cast<v_type_local>(value) * 10 + ( numTmp - uint_c_v<'0'> ));
				++iter;
				numTmp = loadByte<checked>(iter, end);
			} else [[unlikely]] {
				if (!expFracTable[numTmp]) [[likely]] {
					if constexpr (negative) {
						value *= -1;
					}
					return iter;
				}
				if constexpr (negative) {
					return (iter = finishParse(value, iter, end), value *= -1, iter);
				} else {
					return finishParse(value, iter, end);
				}
			}

			if (isDigit(numTmp)) [[likely]] {
				value = static_cast<value_type>(static_cast<v_type_local>(value) * 10 + ( numTmp - uint_c_v<'0'> ));
				++iter;
				numTmp = loadByte<checked>(iter, end);
			} else [[unlikely]] {
				if (!expFracTable[numTmp]) [[likely]] {
					if constexpr (negative) {
						value *= -1;
					}
					return iter;
				}
				if constexpr (negative) {
					return (iter = finishParse(value, iter, end), value *= -1, iter);
				} else {
					return finishParse(value, iter, end);
				}
			}

			if (isDigit(numTmp)) [[likely]] {
				value = static_cast<value_type>(static_cast<v_type_local>(value) * 10 + ( numTmp - uint_c_v<'0'> ));
				++iter;
				numTmp = loadByte<checked>(iter, end);
			} else [[unlikely]] {
				if (!expFracTable[numTmp]) [[likely]] {
					if constexpr (negative) {
						value *= -1;
					}
					return iter;
				}
				if constexpr (negative) {
					return (iter = finishParse(value, iter, end), value *= -1, iter);
				} else {
					return finishParse(value, iter, end);
				}
			}

			if constexpr (uint_types<value_type>) {
				if (isDigit(numTmp)) [[likely]] {
					value = static_cast<value_type>(static_cast<v_type_local>(value) * 10 + ( numTmp - uint_c_v<'0'> ));
					++iter;
					numTmp = loadByte<checked>(iter, end);
				} else [[unlikely]] {
					if (!expFracTable[numTmp]) [[likely]] {
						if constexpr (negative) {
							value *= -1;
						}
						return iter;
					}
					if constexpr (negative) {
						return (iter = finishParse(value, iter, end), value *= -1, iter);
					} else {
						return finishParse(value, iter, end);
					}
				}
			}

			if (isDigit(numTmp)) [[likely]] {
				if (static_cast<uint64_t>(value) > static_cast<uint64_t>(compVals<value_type, negative>[numTmp])) [[unlikely]] {
					return nullptr;
				}
				value = static_cast<value_type>(static_cast<v_type_local>(value) * 10 + ( numTmp - uint_c_v<'0'> ));
				++iter;
				numTmp = loadByte<checked>(iter, end);
			} else [[unlikely]] {
				if (!expFracTable[numTmp]) [[likely]] {
					if constexpr (negative) {
						value *= -1;
					}
					return iter;
				}
				if constexpr (negative) {
					return (iter = finishParse(value, iter, end), value *= -1, iter);
				} else {
					return finishParse(value, iter, end);
				}
			}
			if (isDigit(numTmp)) [[unlikely]] {
				return nullptr;
			}
			static constexpr v_type_local zeroVal{ 0 };
			if (expFracTable[numTmp]) [[unlikely]] {
				if constexpr (negative) {
					return (iter = finishParse(value, iter, end), value = static_cast<value_type>(zeroVal - static_cast<v_type_local>(value)), iter);
				} else {
					return finishParse(value, iter, end);
				}
			}
			if constexpr (negative) {
				value = static_cast<value_type>(zeroVal - static_cast<v_type_local>(value));
			}
			return iter;
		}

		JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> parseInt(value_type& value, read_buffer_ptr iter, read_buffer_ptr end) noexcept {
			if constexpr (int_types<value_type>) {
				if (iter < end) [[likely]] {
					if (*iter == uint_c_v<'-'>) {
						++iter;
						const uint8_t* resultPtr = (end - iter) >= uncheckedIntegerSpan ? parseInteger<true, false>(value, iter, end) : parseInteger<true, true>(value, iter, end);
						if (resultPtr) [[likely]] {
							return resultPtr;
						} else {
							value = 0;
							return nullptr;
						}
					} else {
						const uint8_t* resultPtr =
							(end - iter) >= uncheckedIntegerSpan ? parseInteger<false, false>(value, iter, end) : parseInteger<false, true>(value, iter, end);
						if (resultPtr) [[likely]] {
							return resultPtr;
						} else {
							value = 0;
							return nullptr;
						}
					}
				} else {
					value = 0;
					return nullptr;
				}
			} else {
				if (iter < end) [[likely]] {
					const uint8_t* resultPtr = (end - iter) >= uncheckedIntegerSpan ? parseInteger<false, false>(value, iter, end) : parseInteger<false, true>(value, iter, end);
					if (resultPtr) [[likely]] {
						return resultPtr;
					} else {
						value = 0;
						return nullptr;
					}
				} else {
					value = 0;
					return nullptr;
				}
			}
		}

		JSONIFIER_INLINE static const uint8_t* parseExponent(value_type& value, const uint8_t* iter, int8_t expSign, const uint8_t* end) noexcept {
			if (isDigit(peekByte(iter, end))) [[likely]] {
				value_type expValue{ static_cast<value_type>(peekByte(iter, end) - uint_c_v<'0'>) };
				++iter;
				while (isDigit(peekByte(iter, end))) {
					expValue = expValue < maxExponentValue ? expValue * 10 + static_cast<value_type>(peekByte(iter, end) - uint_c_v<'0'>) : expValue;
					++iter;
				}
				if (expValue < static_cast<value_type>(powers_of_ten<value_type>::valueRaw.size())) [[likely]] {
					const value_type powerExp			  = powers_of_ten<value_type>::value[expValue];
					static constexpr value_type doubleMax = std::numeric_limits<value_type>::max();
					static constexpr value_type doubleMin = std::numeric_limits<value_type>::min();
					expValue *= static_cast<value_type>(expSign);
					return (expSign > 0) ? ((value <= (doubleMax / powerExp)) ? (multiply(value, powerExp), iter) : nullptr)
										 : ((value / powerExp >= (doubleMin)) ? (divide(value, powerExp), iter) : nullptr);
				} else [[unlikely]] {
					return nullptr;
				}
			} else [[unlikely]] {
				return nullptr;
			}
		}

		JSONIFIER_INLINE static const uint8_t* parseExponentPostFrac(value_type& value, const uint8_t* iter, int8_t expSign, value_type fracValue,
			typename get_int_type<value_type>::type fracDigits, const uint8_t* end) noexcept {
			using exp_type = conditional_t<uint_types<value_type>, int64_t, value_type>;
			if (isDigit(peekByte(iter, end))) [[likely]] {
				exp_type expValue{ static_cast<exp_type>(peekByte(iter, end) - uint_c_v<'0'>) };
				++iter;
				while (isDigit(peekByte(iter, end))) {
					expValue = expValue < maxExponentValue ? expValue * 10 + static_cast<exp_type>(peekByte(iter, end) - uint_c_v<'0'>) : expValue;
					++iter;
				}
				if (expValue < static_cast<exp_type>(powers_of_ten<value_type>::valueRaw.size())) [[likely]] {
					const value_type powerExp = powers_of_ten<value_type>::value[expValue];

					static constexpr value_type doubleMax = std::numeric_limits<value_type>::max();
					static constexpr value_type doubleMin = std::numeric_limits<value_type>::min();

					if (fracDigits + expValue >= 0) {
						expValue *= expSign;
						const auto fractionalCorrection = expValue > fracDigits
							? fracValue * powers_of_ten<value_type>::value[expValue - fracDigits]
							: (fracDigits - expValue < static_cast<decltype(fracDigits - expValue)>(powers_of_ten<value_type>::valueRaw.size())
									  ? fracValue / powers_of_ten<value_type>::value[fracDigits - expValue]
									  : value_type{});
						return (expSign > 0) ? ((value <= (doubleMax / powerExp))
													   ? (multiply(value, powerExp), (fractionalCorrection <= doubleMax - value) ? (value += fractionalCorrection, iter) : nullptr)
													   : nullptr)
											 : ((value / powerExp >= (doubleMin)) ? (divide(value, powerExp), value += fractionalCorrection, iter) : nullptr);
					} else {
						return (expSign > 0) ? ((value <= (doubleMax / powerExp)) ? (multiply(value, powerExp), iter) : nullptr)
											 : ((value / powerExp >= (doubleMin)) ? (divide(value, powerExp), iter) : nullptr);
					}
				} else [[unlikely]] {
					return nullptr;
				}
			} else [[unlikely]] {
				return nullptr;
			}
		}

		JSONIFIER_INLINE static const uint8_t* parseFraction(value_type& value, const uint8_t* iter, const uint8_t* end) noexcept {
			if (isDigit(peekByte(iter, end))) [[likely]] {
				value_type fracValue{ static_cast<value_type>(peekByte(iter, end) - uint_c_v<'0'>) };
				typename get_int_type<value_type>::type fracDigits{ 1 };
				++iter;
				while (isDigit(peekByte(iter, end))) {
					if (fracDigits < (uint_types<value_type> ? maxUnsignedFractionDigits : maxFractionDigits)) {
						fracValue = fracValue * 10 + static_cast<value_type>(peekByte(iter, end) - uint_c_v<'0'>);
						++fracDigits;
					}
					++iter;
				}
				if (expTable[peekByte(iter, end)]) {
					++iter;
					int8_t expSign = 1;
					if (peekByte(iter, end) == uint_c_v<'-'>) {
						expSign = -1;
						++iter;
					} else if (peekByte(iter, end) == uint_c_v<'+'>) {
						++iter;
					}
					return parseExponentPostFrac(value, iter, expSign, fracValue, fracDigits, end);
				}
			}
			if (!expFracTable[peekByte(iter, end)]) [[likely]] {
				return iter;
			} else {
				return nullptr;
			}
		}

		static const uint8_t* finishParse(value_type& value, const uint8_t* iter, const uint8_t* end) noexcept {
			if (peekByte(iter, end) == uint_c_v<'.'>) [[unlikely]] {
				++iter;
				return parseFraction(value, iter, end);
			} else if (expTable[peekByte(iter, end)]) {
				++iter;
				int8_t expSign = 1;
				if (peekByte(iter, end) == uint_c_v<'-'>) {
					expSign = -1;
					++iter;
				} else if (peekByte(iter, end) == uint_c_v<'+'>) {
					++iter;
				}
				return parseExponent(value, iter, expSign, end);
			}
			if (!expFracTable[peekByte(iter, end)]) [[likely]] {
				return nullptr;
			} else {
				return nullptr;
			}
		}
	};

#else

template<bool negative> inline constexpr uint64_t compValAddition{ [] {
	if constexpr (negative) {
		return 1ULL;
	} else {
		return 0ULL;
	}
}() };

template<typename v_type, bool negative> consteval array<std::make_unsigned_t<v_type>, 256ULL> genRawCompVals() noexcept {
	constexpr auto maxValue{ static_cast<std::make_unsigned_t<v_type>>(std::numeric_limits<remove_cvref_t<v_type>>::max()) + compValAddition<negative> };
	array<std::make_unsigned_t<v_type>, 256ULL> returnValues{};
	returnValues[uint_c_v<'0'>] = (maxValue - 0) / 10;
	returnValues[uint_c_v<'1'>] = (maxValue - 1) / 10;
	returnValues[uint_c_v<'2'>] = (maxValue - 2) / 10;
	returnValues[uint_c_v<'3'>] = (maxValue - 3) / 10;
	returnValues[uint_c_v<'4'>] = (maxValue - 4) / 10;
	returnValues[uint_c_v<'5'>] = (maxValue - 5) / 10;
	returnValues[uint_c_v<'6'>] = (maxValue - 6) / 10;
	returnValues[uint_c_v<'7'>] = (maxValue - 7) / 10;
	returnValues[uint_c_v<'8'>] = (maxValue - 8) / 10;
	returnValues[uint_c_v<'9'>] = (maxValue - 9) / 10;
	return returnValues;
}

template<typename v_type, bool negative> alignas(64) inline constexpr auto compValsRaw{ genRawCompVals<v_type, negative>() };

template<typename v_type, bool negative> inline constexpr const std::make_unsigned_t<v_type>* __restrict compVals{ compValsRaw<v_type, negative>.data() };

namespace int_swar {

	template<uint8_types auto repeat, uint_types v_type> inline constexpr v_type repeatBytes =
		static_cast<v_type>(static_cast<v_type>(0x0101010101010101ull) * static_cast<v_type>(repeat));

	template<uint_types v_type> JSONIFIER_INLINE v_type countZeros(v_type value) noexcept {
		return static_cast<v_type>(simd::countrZeroUnsafe(value));
	}

	template<integer_types v_type> struct parse_chunk_result {
		v_type value;
		uint64_t digits;
	};

	template<uint_types v_type> JSONIFIER_INLINE v_type load(read_buffer_ptr str) noexcept {
		v_type chunk;
		pow2MemcpyWrapper<sizeof(v_type)>(&chunk, str);
		if constexpr (std::endian::native == std::endian::big) {
			chunk = internal::byteswap(chunk);
		}
		return chunk;
	}

	template<uint8_types v_type> JSONIFIER_INLINE v_type load(read_buffer_ptr str) noexcept {
		return *str;
	}

	template<uint_types v_type> JSONIFIER_INLINE v_type mask(v_type raw) noexcept {
		static constexpr v_type high{ repeatBytes<static_cast<uint8_t>(0x80), v_type> };
		static constexpr v_type low{ repeatBytes<static_cast<uint8_t>(0x7F), v_type> };
		static constexpr v_type up{ repeatBytes<static_cast<uint8_t>(0x46), v_type> };
		static constexpr v_type down{ repeatBytes<static_cast<uint8_t>(0x50), v_type> };
		const v_type body{ static_cast<v_type>(raw & low) };
		const v_type above{ static_cast<v_type>((body + up) & high) };
		const v_type atLeast{ static_cast<v_type>((body + down) & high) };
		return static_cast<v_type>((raw & high) | above | (~atLeast & high));
	}

	JSONIFIER_INLINE bool incorrect(uint8_t raw) noexcept {
		return static_cast<uint8_t>(raw - static_cast<uint8_t>(0x30)) > 9u;
	}

	template<typename v_type> struct fold;

	template<uint64_types v_type> struct fold<v_type> {
		JSONIFIER_INLINE static uint64_t impl(v_type raw) noexcept {
			const v_type sub{ raw - repeatBytes<static_cast<uint8_t>(0x30), v_type> };
			v_type val = (sub * 10 + (sub >> 8)) & 0x00FF00FF00FF00FFULL;
			val		   = (val * 100 + (val >> 16)) & 0x0000FFFF0000FFFFULL;
			return (val * 10000 + (val >> 32)) & 0x00000000FFFFFFFFULL;
		}
	};

	template<uint32_types v_type> struct fold<v_type> {
		JSONIFIER_INLINE static uint64_t impl(v_type raw) noexcept {
			const v_type sub{ static_cast<v_type>(raw - repeatBytes<static_cast<uint8_t>(0x30), v_type>) };
			v_type val = (sub * 10 + (sub >> 8)) & 0x00FF00FFUL;
			return static_cast<uint64_t>((val * 100 + (val >> 16)) & 0x0000FFFFUL);
		}
	};

	template<uint16_types v_type> struct fold<v_type> {
		JSONIFIER_INLINE static uint64_t impl(v_type raw) noexcept {
			const v_type sub{ static_cast<v_type>(raw - repeatBytes<static_cast<uint8_t>(0x30), v_type>) };
			return static_cast<uint64_t>((sub & 0xFFU) * 10 + (sub >> 8));
		}
	};

	template<uint8_types v_type> struct fold<v_type> {
		JSONIFIER_INLINE static uint64_t impl(v_type raw) noexcept {
			return static_cast<uint64_t>(static_cast<v_type>(raw - static_cast<v_type>(0x30)));
		}
	};

	template<integer_types v_type, uint64_t length> struct parse_fixed;

	template<integer_types v_type> struct parse_fixed<v_type, 1ULL> {
		JSONIFIER_INLINE static parse_chunk_result<v_type> impl(read_buffer_ptr str) noexcept {
			const uint8_t s1 = load<uint8_t>(str);
			if (incorrect(s1)) [[unlikely]] {
				return { 0, 0 };
			}
			return { static_cast<v_type>(fold<uint8_t>::impl(s1)), 1 };
		}
	};

	template<integer_types v_type> struct parse_fixed<v_type, 2ULL> {
		JSONIFIER_INLINE static parse_chunk_result<v_type> impl(read_buffer_ptr str) noexcept {
			const uint16_t s2 = load<uint16_t>(str);
			const uint16_t m2 = mask(s2);
			if (m2) [[unlikely]] {
				return { 0, static_cast<uint64_t>(countZeros(m2) >> 3) };
			}
			return { static_cast<v_type>(fold<uint16_t>::impl(s2)), 2 };
		}
	};

	template<integer_types v_type> struct parse_fixed<v_type, 3ULL> {
		JSONIFIER_INLINE static parse_chunk_result<v_type> impl(read_buffer_ptr str) noexcept {
			const uint16_t s2 = load<uint16_t>(str);
			const uint8_t s1  = load<uint8_t>(str + 2);
			const uint16_t m2 = mask(s2);
			if (m2) [[unlikely]] {
				return { 0, static_cast<uint64_t>(countZeros(m2) >> 3) };
			}
			if (incorrect(s1)) [[unlikely]] {
				return { 0, 2 };
			}
			return { static_cast<v_type>(fold<uint16_t>::impl(s2) * 10ULL + fold<uint8_t>::impl(s1)), 3 };
		}
	};

	template<integer_types v_type> struct parse_fixed<v_type, 4ULL> {
		JSONIFIER_INLINE static parse_chunk_result<v_type> impl(read_buffer_ptr str) noexcept {
			const uint32_t s4 = load<uint32_t>(str);
			const uint32_t m4 = mask(s4);
			if (m4) [[unlikely]] {
				return { 0, static_cast<uint64_t>(countZeros(m4) >> 3) };
			}
			return { static_cast<v_type>(fold<uint32_t>::impl(s4)), 4 };
		}
	};

	template<integer_types v_type> struct parse_fixed<v_type, 5ULL> {
		JSONIFIER_INLINE static parse_chunk_result<v_type> impl(read_buffer_ptr str) noexcept {
			const uint32_t s4 = load<uint32_t>(str);
			const uint8_t s1  = load<uint8_t>(str + 4);
			const uint32_t m4 = mask(s4);
			if (m4) [[unlikely]] {
				return { 0, static_cast<uint64_t>(countZeros(m4) >> 3) };
			}
			if (incorrect(s1)) [[unlikely]] {
				return { 0, 4 };
			}
			return { static_cast<v_type>(fold<uint32_t>::impl(s4) * 10ULL + fold<uint8_t>::impl(s1)), 5 };
		}
	};

	template<integer_types v_type> struct parse_fixed<v_type, 6ULL> {
		JSONIFIER_INLINE static parse_chunk_result<v_type> impl(read_buffer_ptr str) noexcept {
			const uint32_t s4 = load<uint32_t>(str);
			const uint16_t s2 = load<uint16_t>(str + 4);
			const uint32_t m4 = mask(s4);
			if (m4) [[unlikely]] {
				return { 0, static_cast<uint64_t>(countZeros(m4) >> 3) };
			}
			const uint16_t m2 = mask(s2);
			if (m2) [[unlikely]] {
				return { 0, 4ULL + static_cast<uint64_t>(countZeros(m2) >> 3) };
			}
			return { static_cast<v_type>(fold<uint32_t>::impl(s4) * 100ULL + fold<uint16_t>::impl(s2)), 6 };
		}
	};

	template<integer_types v_type> struct parse_fixed<v_type, 7ULL> {
		JSONIFIER_INLINE static parse_chunk_result<v_type> impl(read_buffer_ptr str) noexcept {
			const uint32_t s4 = load<uint32_t>(str);
			const uint16_t s2 = load<uint16_t>(str + 4);
			const uint8_t s1  = load<uint8_t>(str + 6);
			const uint32_t m4 = mask(s4);
			if (m4) [[unlikely]] {
				return { 0, static_cast<uint64_t>(countZeros(m4) >> 3) };
			}
			const uint16_t m2 = mask(s2);
			if (m2) [[unlikely]] {
				return { 0, 4ULL + static_cast<uint64_t>(countZeros(m2) >> 3) };
			}
			if (incorrect(s1)) [[unlikely]] {
				return { 0, 6 };
			}
			return { static_cast<v_type>(fold<uint32_t>::impl(s4) * 1000ULL + fold<uint16_t>::impl(s2) * 10ULL + fold<uint8_t>::impl(s1)), 7 };
		}
	};

	template<integer_types v_type> struct parse_fixed<v_type, 8ULL> {
		JSONIFIER_INLINE static parse_chunk_result<v_type> impl(read_buffer_ptr str) noexcept {
			const uint64_t s8 = load<uint64_t>(str);
			const uint64_t m8 = mask(s8);
			if (m8) [[unlikely]] {
				return { 0, countZeros(m8) >> 3 };
			}
			return { static_cast<v_type>(fold<uint64_t>::impl(s8)), 8 };
		}
	};

	template<integer_types v_type> struct parse_fixed<v_type, 9ULL> {
		JSONIFIER_INLINE static parse_chunk_result<v_type> impl(read_buffer_ptr str) noexcept {
			const uint64_t s8 = load<uint64_t>(str);
			const uint8_t s1  = load<uint8_t>(str + 8);
			const uint64_t m8 = mask(s8);
			if (m8) [[unlikely]] {
				return { 0, countZeros(m8) >> 3 };
			}
			if (incorrect(s1)) [[unlikely]] {
				return { 0, 8 };
			}
			return { static_cast<v_type>(fold<uint64_t>::impl(s8) * 10ULL + fold<uint8_t>::impl(s1)), 9 };
		}
	};

	template<integer_types v_type> struct parse_fixed<v_type, 10ULL> {
		JSONIFIER_INLINE static parse_chunk_result<v_type> impl(read_buffer_ptr str) noexcept {
			const uint64_t s8 = load<uint64_t>(str);
			const uint16_t s2 = load<uint16_t>(str + 8);
			const uint64_t m8 = mask(s8);
			if (m8) [[unlikely]] {
				return { 0, countZeros(m8) >> 3 };
			}
			const uint16_t m2 = mask(s2);
			if (m2) [[unlikely]] {
				return { 0, 8ULL + static_cast<uint64_t>(countZeros(m2) >> 3) };
			}
			return { static_cast<v_type>(fold<uint64_t>::impl(s8) * 100ULL + fold<uint16_t>::impl(s2)), 10 };
		}
	};

	template<integer_types v_type> struct parse_fixed<v_type, 11ULL> {
		JSONIFIER_INLINE static parse_chunk_result<v_type> impl(read_buffer_ptr str) noexcept {
			const uint64_t s8 = load<uint64_t>(str);
			const uint16_t s2 = load<uint16_t>(str + 8);
			const uint8_t s1  = load<uint8_t>(str + 10);
			const uint64_t m8 = mask(s8);
			if (m8) [[unlikely]] {
				return { 0, countZeros(m8) >> 3 };
			}
			const uint16_t m2 = mask(s2);
			if (m2) [[unlikely]] {
				return { 0, 8ULL + static_cast<uint64_t>(countZeros(m2) >> 3) };
			}
			if (incorrect(s1)) [[unlikely]] {
				return { 0, 10 };
			}
			return { static_cast<v_type>(fold<uint64_t>::impl(s8) * 1000ULL + fold<uint16_t>::impl(s2) * 10ULL + fold<uint8_t>::impl(s1)), 11 };
		}
	};

	template<integer_types v_type> struct parse_fixed<v_type, 12ULL> {
		JSONIFIER_INLINE static parse_chunk_result<v_type> impl(read_buffer_ptr str) noexcept {
			const uint64_t s8 = load<uint64_t>(str);
			const uint32_t s4 = load<uint32_t>(str + 8);
			const uint64_t m8 = mask(s8);
			if (m8) [[unlikely]] {
				return { 0, countZeros(m8) >> 3 };
			}
			const uint32_t m4 = mask(s4);
			if (m4) [[unlikely]] {
				return { 0, 8ULL + static_cast<uint64_t>(countZeros(m4) >> 3) };
			}
			return { static_cast<v_type>(fold<uint64_t>::impl(s8) * 10000ULL + fold<uint32_t>::impl(s4)), 12 };
		}
	};

	template<integer_types v_type> struct parse_fixed<v_type, 13ULL> {
		JSONIFIER_INLINE static parse_chunk_result<v_type> impl(read_buffer_ptr str) noexcept {
			const uint64_t s8 = load<uint64_t>(str);
			const uint32_t s4 = load<uint32_t>(str + 8);
			const uint8_t s1  = load<uint8_t>(str + 12);
			const uint64_t m8 = mask(s8);
			if (m8) [[unlikely]] {
				return { 0, countZeros(m8) >> 3 };
			}
			const uint32_t m4 = mask(s4);
			if (m4) [[unlikely]] {
				return { 0, 8ULL + static_cast<uint64_t>(countZeros(m4) >> 3) };
			}
			if (incorrect(s1)) [[unlikely]] {
				return { 0, 12 };
			}
			return { static_cast<v_type>(fold<uint64_t>::impl(s8) * 100000ULL + fold<uint32_t>::impl(s4) * 10ULL + fold<uint8_t>::impl(s1)), 13 };
		}
	};

	template<integer_types v_type> struct parse_fixed<v_type, 14ULL> {
		JSONIFIER_INLINE static parse_chunk_result<v_type> impl(read_buffer_ptr str) noexcept {
			const uint64_t s8 = load<uint64_t>(str);
			const uint32_t s4 = load<uint32_t>(str + 8);
			const uint16_t s2 = load<uint16_t>(str + 12);
			const uint64_t m8 = mask(s8);
			if (m8) [[unlikely]] {
				return { 0, countZeros(m8) >> 3 };
			}
			const uint32_t m4 = mask(s4);
			if (m4) [[unlikely]] {
				return { 0, 8ULL + static_cast<uint64_t>(countZeros(m4) >> 3) };
			}
			const uint16_t m2 = mask(s2);
			if (m2) [[unlikely]] {
				return { 0, 12ULL + static_cast<uint64_t>(countZeros(m2) >> 3) };
			}
			return { static_cast<v_type>(fold<uint64_t>::impl(s8) * 1000000ULL + fold<uint32_t>::impl(s4) * 100ULL + fold<uint16_t>::impl(s2)), 14 };
		}
	};

	template<integer_types v_type> struct parse_fixed<v_type, 15ULL> {
		JSONIFIER_INLINE static parse_chunk_result<v_type> impl(read_buffer_ptr str) noexcept {
			const uint64_t s8 = load<uint64_t>(str);
			const uint32_t s4 = load<uint32_t>(str + 8);
			const uint16_t s2 = load<uint16_t>(str + 12);
			const uint8_t s1  = load<uint8_t>(str + 14);
			const uint64_t m8 = mask(s8);
			if (m8) [[unlikely]] {
				return { 0, countZeros(m8) >> 3 };
			}
			const uint32_t m4 = mask(s4);
			if (m4) [[unlikely]] {
				return { 0, 8ULL + static_cast<uint64_t>(countZeros(m4) >> 3) };
			}
			const uint16_t m2 = mask(s2);
			if (m2) [[unlikely]] {
				return { 0, 12ULL + static_cast<uint64_t>(countZeros(m2) >> 3) };
			}
			if (incorrect(s1)) [[unlikely]] {
				return { 0, 14 };
			}
			return { static_cast<v_type>(fold<uint64_t>::impl(s8) * 10000000ULL + fold<uint32_t>::impl(s4) * 1000ULL + fold<uint16_t>::impl(s2) * 10ULL + fold<uint8_t>::impl(s1)),
				15 };
		}
	};

	template<integer_types v_type> struct parse_fixed<v_type, 16ULL> {
		JSONIFIER_INLINE static parse_chunk_result<v_type> impl(read_buffer_ptr str) noexcept {
			const uint64_t s8a = load<uint64_t>(str);
			const uint64_t s8b = load<uint64_t>(str + 8);
			const uint64_t m8a = mask(s8a);
			if (m8a) [[unlikely]] {
				return { 0, countZeros(m8a) >> 3 };
			}
			const uint64_t m8b = mask(s8b);
			if (m8b) [[unlikely]] {
				return { 0, 8ULL + (countZeros(m8b) >> 3) };
			}
			return { static_cast<v_type>(fold<uint64_t>::impl(s8a) * 100000000ULL + fold<uint64_t>::impl(s8b)), 16 };
		}
	};

	template<integer_types v_type> struct parse_fixed<v_type, 17ULL> {
		JSONIFIER_INLINE static parse_chunk_result<v_type> impl(read_buffer_ptr str) noexcept {
			const uint64_t s8a = load<uint64_t>(str);
			const uint64_t s8b = load<uint64_t>(str + 8);
			const uint8_t s1   = load<uint8_t>(str + 16);
			const uint64_t m8a = mask(s8a);
			if (m8a) [[unlikely]] {
				return { 0, countZeros(m8a) >> 3 };
			}
			const uint64_t m8b = mask(s8b);
			if (m8b) [[unlikely]] {
				return { 0, 8ULL + (countZeros(m8b) >> 3) };
			}
			if (incorrect(s1)) [[unlikely]] {
				return { 0, 16 };
			}
			return { static_cast<v_type>(fold<uint64_t>::impl(s8a) * 1000000000ULL + fold<uint64_t>::impl(s8b) * 10ULL + fold<uint8_t>::impl(s1)), 17 };
		}
	};

	template<integer_types v_type> struct parse_fixed<v_type, 18ULL> {
		JSONIFIER_INLINE static parse_chunk_result<v_type> impl(read_buffer_ptr str) noexcept {
			const uint64_t s8a = load<uint64_t>(str);
			const uint64_t s8b = load<uint64_t>(str + 8);
			const uint16_t s2  = load<uint16_t>(str + 16);
			const uint64_t m8a = mask(s8a);
			if (m8a) [[unlikely]] {
				return { 0, countZeros(m8a) >> 3 };
			}
			const uint64_t m8b = mask(s8b);
			if (m8b) [[unlikely]] {
				return { 0, 8ULL + (countZeros(m8b) >> 3) };
			}
			const uint16_t m2 = mask(s2);
			if (m2) [[unlikely]] {
				return { 0, 16ULL + static_cast<uint64_t>(countZeros(m2) >> 3) };
			}
			return { static_cast<v_type>(fold<uint64_t>::impl(s8a) * 10000000000ULL + fold<uint64_t>::impl(s8b) * 100ULL + fold<uint16_t>::impl(s2)), 18 };
		}
	};

	template<integer_types v_type> struct parse_fixed<v_type, 19ULL> {
		JSONIFIER_INLINE static parse_chunk_result<v_type> impl(read_buffer_ptr str) noexcept {
			const uint64_t s8a = load<uint64_t>(str);
			const uint64_t s8b = load<uint64_t>(str + 8);
			const uint16_t s2  = load<uint16_t>(str + 16);
			const uint8_t s1   = load<uint8_t>(str + 18);
			const uint64_t m8a = mask(s8a);
			if (m8a) [[unlikely]] {
				return { 0, countZeros(m8a) >> 3 };
			}
			const uint64_t m8b = mask(s8b);
			if (m8b) [[unlikely]] {
				return { 0, 8ULL + (countZeros(m8b) >> 3) };
			}
			const uint16_t m2 = mask(s2);
			if (m2) [[unlikely]] {
				return { 0, 16ULL + static_cast<uint64_t>(countZeros(m2) >> 3) };
			}
			if (incorrect(s1)) [[unlikely]] {
				return { 0, 18 };
			}
			return { static_cast<v_type>(
						 fold<uint64_t>::impl(s8a) * 100000000000ULL + fold<uint64_t>::impl(s8b) * 1000ULL + fold<uint16_t>::impl(s2) * 10ULL + fold<uint8_t>::impl(s1)),
				19 };
		}
	};

	template<integer_types v_type> struct parse_fixed<v_type, 20ULL> {
		JSONIFIER_INLINE static parse_chunk_result<v_type> impl(read_buffer_ptr str) noexcept {
			const uint64_t s8a = load<uint64_t>(str);
			const uint64_t s8b = load<uint64_t>(str + 8);
			const uint32_t s4  = load<uint32_t>(str + 16);
			const uint64_t m8a = mask(s8a);
			if (m8a) [[unlikely]] {
				return { 0, countZeros(m8a) >> 3 };
			}
			const uint64_t m8b = mask(s8b);
			if (m8b) [[unlikely]] {
				return { 0, 8ULL + (countZeros(m8b) >> 3) };
			}
			const uint32_t m4 = mask(s4);
			if (m4) [[unlikely]] {
				return { 0, 16ULL + static_cast<uint64_t>(countZeros(m4) >> 3) };
			}
			return { static_cast<v_type>(fold<uint64_t>::impl(s8a) * 1000000000000ULL + fold<uint64_t>::impl(s8b) * 10000ULL + fold<uint32_t>::impl(s4)), 20 };
		}
	};

	template<integer_types v_type, uint64_t maxLength> struct dispatch_table;

	template<integer_types v_type> struct dispatch_table<v_type, 19ULL> {
		JSONIFIER_INLINE static parse_chunk_result<v_type> impl(read_buffer_ptr str, uint64_t length) noexcept {
			switch (length) {
				case 1:
					return parse_fixed<v_type, 1ULL>::impl(str);
				case 2:
					return parse_fixed<v_type, 2ULL>::impl(str);
				case 3:
					return parse_fixed<v_type, 3ULL>::impl(str);
				case 4:
					return parse_fixed<v_type, 4ULL>::impl(str);
				case 5:
					return parse_fixed<v_type, 5ULL>::impl(str);
				case 6:
					return parse_fixed<v_type, 6ULL>::impl(str);
				case 7:
					return parse_fixed<v_type, 7ULL>::impl(str);
				case 8:
					return parse_fixed<v_type, 8ULL>::impl(str);
				case 9:
					return parse_fixed<v_type, 9ULL>::impl(str);
				case 10:
					return parse_fixed<v_type, 10ULL>::impl(str);
				case 11:
					return parse_fixed<v_type, 11ULL>::impl(str);
				case 12:
					return parse_fixed<v_type, 12ULL>::impl(str);
				case 13:
					return parse_fixed<v_type, 13ULL>::impl(str);
				case 14:
					return parse_fixed<v_type, 14ULL>::impl(str);
				case 15:
					return parse_fixed<v_type, 15ULL>::impl(str);
				case 16:
					return parse_fixed<v_type, 16ULL>::impl(str);
				case 17:
					return parse_fixed<v_type, 17ULL>::impl(str);
				case 18:
					return parse_fixed<v_type, 18ULL>::impl(str);
				default:
					return parse_fixed<v_type, 19ULL>::impl(str);
			}
		}
	};

	template<integer_types v_type> struct dispatch_table<v_type, 20ULL> {
		JSONIFIER_INLINE static parse_chunk_result<v_type> impl(read_buffer_ptr str, uint64_t length) noexcept {
			switch (length) {
				case 1:
					return parse_fixed<v_type, 1ULL>::impl(str);
				case 2:
					return parse_fixed<v_type, 2ULL>::impl(str);
				case 3:
					return parse_fixed<v_type, 3ULL>::impl(str);
				case 4:
					return parse_fixed<v_type, 4ULL>::impl(str);
				case 5:
					return parse_fixed<v_type, 5ULL>::impl(str);
				case 6:
					return parse_fixed<v_type, 6ULL>::impl(str);
				case 7:
					return parse_fixed<v_type, 7ULL>::impl(str);
				case 8:
					return parse_fixed<v_type, 8ULL>::impl(str);
				case 9:
					return parse_fixed<v_type, 9ULL>::impl(str);
				case 10:
					return parse_fixed<v_type, 10ULL>::impl(str);
				case 11:
					return parse_fixed<v_type, 11ULL>::impl(str);
				case 12:
					return parse_fixed<v_type, 12ULL>::impl(str);
				case 13:
					return parse_fixed<v_type, 13ULL>::impl(str);
				case 14:
					return parse_fixed<v_type, 14ULL>::impl(str);
				case 15:
					return parse_fixed<v_type, 15ULL>::impl(str);
				case 16:
					return parse_fixed<v_type, 16ULL>::impl(str);
				case 17:
					return parse_fixed<v_type, 17ULL>::impl(str);
				case 18:
					return parse_fixed<v_type, 18ULL>::impl(str);
				case 19:
					return parse_fixed<v_type, 19ULL>::impl(str);
				default:
					return parse_fixed<v_type, 20ULL>::impl(str);
			}
		}
	};

	template<bool negative, integer_types v_type> JSONIFIER_INLINE bool exceedsLimit(v_type value, uint8_t last) noexcept {
		return static_cast<uint64_t>(value) > static_cast<uint64_t>(compVals<v_type, negative>[last]);
	}

}

template<typename value_type, bool negative> JSONIFIER_INLINE unrestrict_t<read_buffer_ptr> parseSwarDigits(value_type& value, read_buffer_ptr iter, read_buffer_ptr end) noexcept {
	static_assert(sizeof(value_type) == 8, "parseSwarDigits only supports 64-bit integers.");
	using v_type_local = std::make_unsigned_t<value_type>;
	static constexpr uint64_t maxDigits{ uint_types<value_type> ? 20ULL : 19ULL };

	if (iter >= end || !isDigit(*iter)) [[unlikely]] {
		return nullptr;
	}
	if (*iter == uint_c_v<'0'> && iter + 1 < end && isDigit(iter[1])) [[unlikely]] {
		return nullptr;
	}

	const uint64_t avail{ static_cast<uint64_t>(end - iter) };
	const uint64_t length{ avail < maxDigits ? avail : maxDigits };
	int_swar::parse_chunk_result<value_type> res{};

	if (length == maxDigits) {
		res = int_swar::dispatch_table<value_type, maxDigits>::impl(iter, length - 1);
		if (res.digits == length - 1) {
			const uint8_t last{ iter[length - 1] };
			if (!int_swar::incorrect(last)) {
				if (static_cast<uint64_t>(res.value) > static_cast<uint64_t>(compVals<value_type, negative>[last])) [[unlikely]] {
					return nullptr;
				}
				res.value  = static_cast<value_type>(static_cast<v_type_local>(res.value) * 10 + static_cast<v_type_local>(last - uint_c_v<'0'>));
				res.digits = length;
			}
		}
	} else {
		res = int_swar::dispatch_table<value_type, maxDigits>::impl(iter, length);
	}

	if (res.digits != length) {
		res = int_swar::dispatch_table<value_type, maxDigits>::impl(iter, res.digits);
	}

	iter += res.digits;
	if (iter < end && isDigit(*iter)) [[unlikely]] {
		return nullptr;
	}
	value = res.value;
	return iter;
}

template<typename value_type, uint64_t size> consteval array<value_type, size> genPowerOfTenTable() noexcept {
	array<value_type, size> returnValues{};
	value_type value{ 1 };
	for (uint64_t x = 0; x < size; ++x) {
		returnValues[x] = value;
		if (x + 1 < size) {
			value *= 10;
		}
	}
	return returnValues;
}

template<typename> struct powers_of_ten;

template<int_types value_type> struct powers_of_ten<value_type> {
	alignas(64) static constexpr array<int64_t, 19> valueRaw{ genPowerOfTenTable<int64_t, 19ULL>() };

	static constexpr const int64_t* __restrict value{ valueRaw.data() };
};

template<uint_types value_type> struct powers_of_ten<value_type> {
	alignas(64) static constexpr array<uint64_t, 20> valueRaw{ genPowerOfTenTable<uint64_t, 20ULL>() };

	static constexpr const uint64_t* __restrict value{ valueRaw.data() };
};

JSONIFIER_INLINE uint8_t peekByte(read_buffer_ptr iter, read_buffer_ptr end) noexcept {
	return iter < end ? *iter : uint8_t{};
}

JSONIFIER_INLINE unrestrict_t<read_buffer_ptr> parseExponentDigits(int64_t& expValue, read_buffer_ptr iter, read_buffer_ptr end) noexcept {
	expValue = 0;
	while (isDigit(peekByte(iter, end))) {
		if (expValue < 1000) {
			expValue = expValue * 10 + (*iter - uint_c_v<'0'>);
		}
		++iter;
	}
	return iter;
}

template<typename value_type> struct integer_parser;

template<int_types value_type> struct integer_parser<value_type> {
	template<bool negative> JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> parseInteger(value_type& value, read_buffer_ptr iter, read_buffer_ptr end) noexcept {
		using v_type_local = std::make_unsigned_t<value_type>;
		iter			   = parseSwarDigits<value_type, negative>(value, iter, end);
		if (!iter) [[unlikely]] {
			return nullptr;
		}
		if (iter < end && expFracTable[*iter]) [[unlikely]] {
			iter = finishParse(value, iter, end);
		}
		if constexpr (negative) {
			value = static_cast<value_type>(static_cast<v_type_local>(0) - static_cast<v_type_local>(value));
		}
		return iter;
	}

	JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> parseExponent(value_type& value, read_buffer_ptr iter, read_buffer_ptr end, int8_t expSign) noexcept {
		if (!isDigit(peekByte(iter, end))) [[unlikely]] {
			return nullptr;
		}
		int64_t expValue;
		iter = parseExponentDigits(expValue, iter, end);
		if (expValue >= 19) [[unlikely]] {
			return nullptr;
		}
		const value_type powerExp			  = powers_of_ten<value_type>::value[expValue];
		static constexpr value_type doubleMax = std::numeric_limits<value_type>::max();
		static constexpr value_type doubleMin = std::numeric_limits<value_type>::min();
		return (expSign > 0) ? ((value <= (doubleMax / powerExp)) ? (multiply(value, powerExp), iter) : nullptr)
							 : ((value / powerExp >= (doubleMin)) ? (divide(value, powerExp), iter) : nullptr);
	}

	JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> parseExponentPostFrac(value_type& value, read_buffer_ptr iter, read_buffer_ptr end, int8_t expSign, value_type fracValue,
		int64_t fracDigits) noexcept {
		if (!isDigit(peekByte(iter, end))) [[unlikely]] {
			return nullptr;
		}
		int64_t expValue;
		iter = parseExponentDigits(expValue, iter, end);
		if (expValue >= 19) [[unlikely]] {
			return nullptr;
		}
		const value_type powerExp			  = powers_of_ten<value_type>::value[expValue];
		static constexpr value_type doubleMax = std::numeric_limits<value_type>::max();
		static constexpr value_type doubleMin = std::numeric_limits<value_type>::min();
		const int64_t shift					  = expSign * expValue - fracDigits;
		const value_type fractionalCorrection =
			shift >= 0 ? fracValue * powers_of_ten<value_type>::value[shift] : (-shift < 19 ? fracValue / powers_of_ten<value_type>::value[-shift] : value_type{});
		if (expSign > 0) {
			if (value > doubleMax / powerExp) [[unlikely]] {
				return nullptr;
			}
			multiply(value, powerExp);
			if (fractionalCorrection > doubleMax - value) [[unlikely]] {
				return nullptr;
			}
			value += fractionalCorrection;
			return iter;
		}
		if (value / powerExp < doubleMin) [[unlikely]] {
			return nullptr;
		}
		divide(value, powerExp);
		value += fractionalCorrection;
		return iter;
	}

	JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> parseFraction(value_type& value, read_buffer_ptr iter, read_buffer_ptr end) noexcept {
		if (isDigit(peekByte(iter, end))) [[likely]] {
			value_type fracValue{};
			int64_t fracDigits{};
			while (isDigit(peekByte(iter, end))) {
				if (fracDigits < 18) {
					fracValue = fracValue * 10 + static_cast<value_type>(*iter - uint_c_v<'0'>);
					++fracDigits;
				}
				++iter;
			}
			if (expTable[peekByte(iter, end)]) {
				++iter;
				int8_t expSign = 1;
				if (peekByte(iter, end) == uint_c_v<'-'>) {
					expSign = -1;
					++iter;
				} else if (peekByte(iter, end) == uint_c_v<'+'>) {
					++iter;
				}
				return parseExponentPostFrac(value, iter, end, expSign, fracValue, fracDigits);
			}
		}
		if (!expFracTable[peekByte(iter, end)]) [[likely]] {
			return iter;
		} else {
			return nullptr;
		}
	}

	JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> finishParse(value_type& value, read_buffer_ptr iter, read_buffer_ptr end) noexcept {
		if (peekByte(iter, end) == uint_c_v<'.'>) [[unlikely]] {
			++iter;
			return parseFraction(value, iter, end);
		} else if (expTable[peekByte(iter, end)]) {
			++iter;
			int8_t expSign = 1;
			if (peekByte(iter, end) == uint_c_v<'-'>) {
				expSign = -1;
				++iter;
			} else if (peekByte(iter, end) == uint_c_v<'+'>) {
				++iter;
			}
			return parseExponent(value, iter, end, expSign);
		}
		if (!expFracTable[peekByte(iter, end)]) [[likely]] {
			return nullptr;
		} else {
			return nullptr;
		}
	}

	JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> parseInt(value_type& value, read_buffer_ptr iter, read_buffer_ptr end) noexcept {
		if (iter < end) [[likely]] {
			if (*iter == uint_c_v<'-'>) {
				++iter;
				read_buffer_ptr resultPtr = parseInteger<true>(value, std::bit_cast<read_buffer_ptr>(iter), std::bit_cast<read_buffer_ptr>(end));
				if (resultPtr) [[likely]] {
					iter += resultPtr - std::bit_cast<read_buffer_ptr>(iter);
					return iter;
				} else {
					value = 0;
					return nullptr;
				}
			} else {
				read_buffer_ptr resultPtr = parseInteger<false>(value, std::bit_cast<read_buffer_ptr>(iter), std::bit_cast<read_buffer_ptr>(end));
				if (resultPtr) [[likely]] {
					iter += resultPtr - std::bit_cast<read_buffer_ptr>(iter);
					return iter;
				} else {
					value = 0;
					return nullptr;
				}
			}
		} else {
			value = 0;
			return nullptr;
		}
	}

	JSONIFIER_INLINE static value_type mul128Generic(value_type ab, value_type cd, value_type& hi) noexcept {
		value_type aHigh = ab >> 32;
		value_type aLow	 = ab & 0xFFFFFFFF;
		value_type bHigh = cd >> 32;
		value_type bLow	 = cd & 0xFFFFFFFF;
		value_type loLo	 = aLow * bLow;
		value_type loHi	 = aLow * bHigh;
		value_type hiLo	 = aHigh * bLow;
		value_type hiHi	 = aHigh * bHigh;
		value_type cross = (loLo >> 32) + (loHi & 0xFFFFFFFF) + (hiLo & 0xFFFFFFFF);
		value_type lo	 = (cross << 32) | (loLo & 0xFFFFFFFF);
		hi				 = hiHi + (loHi >> 32) + (hiLo >> 32) + (cross >> 32);
		return lo;
	}

	JSONIFIER_INLINE static bool multiply(value_type& value, value_type expValue) noexcept {
	#if JSONIFIER_HAS_INT128
		const __int128_t res = static_cast<__int128_t>(value) * static_cast<__int128_t>(expValue);
		value				 = static_cast<value_type>(res);
		return res <= std::numeric_limits<value_type>::max();
	#elif JSONIFIER_COMPILER_MSVC && JSONIFIER_ARCH_X64
		value_type values;
		value = _mul128(value, expValue, &values);
		return values == 0;
	#elif JSONIFIER_COMPILER_CLANG || JSONIFIER_COMPILER_GCC
		return !__builtin_mul_overflow(value, expValue, &value);
	#else
		value_type values;
		value = mul128Generic(value, expValue, values);
		return values == 0;
	#endif
	}

	JSONIFIER_INLINE static bool divide(value_type& value, value_type expValue) noexcept {
	#if JSONIFIER_HAS_INT128
		const __int128_t dividend = static_cast<__int128_t>(value);
		value					  = static_cast<value_type>(dividend / static_cast<__int128_t>(expValue));
		return (dividend % static_cast<__int128_t>(expValue)) == 0;
	#elif JSONIFIER_COMPILER_MSVC && JSONIFIER_ARCH_X64
		value_type values;
		value = _div128(0, value, expValue, &values);
		return values == 0;
	#else
		value_type values;
		values = value % expValue;
		value  = value / expValue;
		return values == 0;
	#endif
	}
};

template<uint_types value_type> struct integer_parser<value_type> {
	JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> parseExponent(value_type& value, read_buffer_ptr iter, read_buffer_ptr end, int8_t expSign) noexcept {
		if (!isDigit(peekByte(iter, end))) [[unlikely]] {
			return nullptr;
		}
		int64_t expValue;
		iter = parseExponentDigits(expValue, iter, end);
		if (expValue > 19) [[unlikely]] {
			return nullptr;
		}
		const value_type powerExp			  = powers_of_ten<value_type>::value[expValue];
		static constexpr value_type doubleMax = std::numeric_limits<value_type>::max();
		static constexpr value_type doubleMin = std::numeric_limits<value_type>::min();
		return (expSign > 0) ? ((value <= (doubleMax / powerExp)) ? (multiply(value, powerExp), iter) : nullptr)
							 : ((value / powerExp >= (doubleMin)) ? (divide(value, powerExp), iter) : nullptr);
	}

	JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> parseExponentPostFrac(value_type& value, read_buffer_ptr iter, read_buffer_ptr end, int8_t expSign, value_type fracValue,
		int64_t fracDigits) noexcept {
		if (!isDigit(peekByte(iter, end))) [[unlikely]] {
			return nullptr;
		}
		int64_t expValue;
		iter = parseExponentDigits(expValue, iter, end);
		if (expValue > 19) [[unlikely]] {
			return nullptr;
		}
		const value_type powerExp			  = powers_of_ten<value_type>::value[expValue];
		static constexpr value_type doubleMax = std::numeric_limits<value_type>::max();
		static constexpr value_type doubleMin = std::numeric_limits<value_type>::min();
		const int64_t shift					  = expSign * expValue - fracDigits;
		const value_type fractionalCorrection =
			shift >= 0 ? fracValue * powers_of_ten<value_type>::value[shift] : (-shift < 20 ? fracValue / powers_of_ten<value_type>::value[-shift] : value_type{});
		if (expSign > 0) {
			if (value > doubleMax / powerExp) [[unlikely]] {
				return nullptr;
			}
			multiply(value, powerExp);
			if (fractionalCorrection > doubleMax - value) [[unlikely]] {
				return nullptr;
			}
			value += fractionalCorrection;
			return iter;
		}
		if (value / powerExp < doubleMin) [[unlikely]] {
			return nullptr;
		}
		divide(value, powerExp);
		value += fractionalCorrection;
		return iter;
	}

	JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> parseFraction(value_type& value, read_buffer_ptr iter, read_buffer_ptr end) noexcept {
		if (isDigit(peekByte(iter, end))) [[likely]] {
			value_type fracValue{};
			int64_t fracDigits{};
			while (isDigit(peekByte(iter, end))) {
				if (fracDigits < 19) {
					fracValue = fracValue * 10 + static_cast<value_type>(*iter - uint_c_v<'0'>);
					++fracDigits;
				}
				++iter;
			}
			if (expTable[peekByte(iter, end)]) {
				++iter;
				int8_t expSign = 1;
				if (peekByte(iter, end) == uint_c_v<'-'>) {
					expSign = -1;
					++iter;
				} else if (peekByte(iter, end) == uint_c_v<'+'>) {
					++iter;
				}
				return parseExponentPostFrac(value, iter, end, expSign, fracValue, fracDigits);
			}
		}
		if (!expFracTable[peekByte(iter, end)]) [[likely]] {
			return iter;
		} else {
			return nullptr;
		}
	}

	JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> parseInteger(value_type& value, read_buffer_ptr iter, read_buffer_ptr end) noexcept {
		iter = parseSwarDigits<value_type, false>(value, iter, end);
		if (!iter) [[unlikely]] {
			return nullptr;
		}
		if (iter < end && expFracTable[*iter]) [[unlikely]] {
			return finishParse(value, iter, end);
		}
		return iter;
	}

	JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> parseInt(value_type& value, read_buffer_ptr iter, read_buffer_ptr end) noexcept {
		if (iter < end) [[likely]] {
			read_buffer_ptr resultPtr = parseInteger(value, std::bit_cast<read_buffer_ptr>(iter), std::bit_cast<read_buffer_ptr>(end));
			if (resultPtr) [[likely]] {
				iter += resultPtr - std::bit_cast<read_buffer_ptr>(iter);
				return iter;
			} else {
				value = 0;
				return nullptr;
			}
		} else {
			value = 0;
			return nullptr;
		}
	}

	inline static unrestrict_t<read_buffer_ptr> finishParse(value_type& value, read_buffer_ptr iter, read_buffer_ptr end) noexcept {
		if (peekByte(iter, end) == uint_c_v<'.'>) [[unlikely]] {
			++iter;
			return parseFraction(value, iter, end);
		} else if (expTable[peekByte(iter, end)]) {
			++iter;
			int8_t expSign = 1;
			if (peekByte(iter, end) == uint_c_v<'-'>) {
				expSign = -1;
				++iter;
			} else if (peekByte(iter, end) == uint_c_v<'+'>) {
				++iter;
			}
			return parseExponent(value, iter, end, expSign);
		}
		if (!expFracTable[peekByte(iter, end)]) [[likely]] {
			return iter;
		} else {
			return nullptr;
		}
	}

	JSONIFIER_INLINE static value_type umul128Generic(value_type ab, value_type cd, value_type& hi) noexcept {
		value_type aHigh = ab >> 32;
		value_type aLow	 = ab & 0xFFFFFFFF;
		value_type bHigh = cd >> 32;
		value_type bLow	 = cd & 0xFFFFFFFF;
		value_type loLo	 = aLow * bLow;
		value_type loHi	 = aLow * bHigh;
		value_type hiLo	 = aHigh * bLow;
		value_type hiHi	 = aHigh * bHigh;
		value_type cross = (loLo >> 32) + (loHi & 0xFFFFFFFF) + (hiLo & 0xFFFFFFFF);
		value_type lo	 = (cross << 32) | (loLo & 0xFFFFFFFF);
		hi				 = hiHi + (loHi >> 32) + (hiLo >> 32) + (cross >> 32);
		return lo;
	}

	JSONIFIER_INLINE static bool multiply(value_type& value, value_type expValue) noexcept {
	#if JSONIFIER_HAS_INT128
		const __uint128_t res = static_cast<__uint128_t>(value) * static_cast<__uint128_t>(expValue);
		value				  = static_cast<value_type>(res);
		return res <= std::numeric_limits<value_type>::max();
	#elif JSONIFIER_COMPILER_MSVC && JSONIFIER_ARCH_X64
		value_type values;
		value = _umul128(value, expValue, &values);
		return values == 0;
	#elif JSONIFIER_COMPILER_CLANG || JSONIFIER_COMPILER_GCC
		return !__builtin_mul_overflow(value, expValue, &value);
	#else
		value_type values;
		value = umul128Generic(value, expValue, values);
		return values == 0;
	#endif
	}

	JSONIFIER_INLINE static bool divide(value_type& value, value_type expValue) noexcept {
	#if JSONIFIER_HAS_INT128
		const __uint128_t dividend = static_cast<__uint128_t>(value);
		value					   = static_cast<value_type>(dividend / static_cast<__uint128_t>(expValue));
		return (dividend % static_cast<__uint128_t>(expValue)) == 0;
	#elif JSONIFIER_COMPILER_MSVC && JSONIFIER_ARCH_X64
		value_type values;
		value = _udiv128(0, value, expValue, &values);
		return values == 0;
	#else
		value_type values;
		values = value % expValue;
		value  = value / expValue;
		return values == 0;
	#endif
	}
};

#endif
}
