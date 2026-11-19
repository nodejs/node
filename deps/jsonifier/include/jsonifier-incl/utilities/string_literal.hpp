/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/utilities/string_literal.hpp
 */
#pragma once

#include <jsonifier-incl/core/config.hpp>
#include <jsonifier-incl/core/fastio.hpp>

namespace jsonifier::internal {

	template<uint64_t sizeVal> struct string_literal {
		using value_type	  = char;
		using size_type		  = uint64_t;
		using reference		  = value_type&;
		using const_reference = const value_type&;
		using pointer		  = value_type*;
		using const_pointer	  = const value_type*;
		static constexpr size_type length{ sizeVal > 0 ? sizeVal - 1 : 0 };

		JSONIFIER_INLINE constexpr string_literal() noexcept = default;

		JSONIFIER_INLINE constexpr string_literal(const char (&str)[sizeVal]) noexcept {
			for (uint64_t x = 0; x < length; ++x) {
				values[x] = str[x];
			}
			values[length] = '\0';
		}

		template<size_type sizeNew> JSONIFIER_INLINE friend constexpr auto operator+(const value_type (&lhs)[sizeNew], const string_literal<sizeVal>& str) noexcept {
			return string_literal<sizeNew>{ lhs } + str;
		}

		template<size_type sizeNew> JSONIFIER_INLINE constexpr string_literal<sizeNew + sizeVal - 1> operator+=(const string_literal<sizeNew>& str) const noexcept {
			string_literal<sizeNew + sizeVal - 1> newLiteral{};
			std::copy(values, values + size(), newLiteral.data());
			std::copy(str.data(), str.data() + sizeNew, newLiteral.data() + size());
			return newLiteral;
		}

		template<size_type sizeNew> JSONIFIER_INLINE constexpr string_literal<sizeNew + sizeVal - 1> operator+(const string_literal<sizeNew>& str) const noexcept {
			string_literal<sizeNew + sizeVal - 1> newLiteral{};
			std::copy(values, values + size(), newLiteral.data());
			std::copy(str.data(), str.data() + sizeNew, newLiteral.data() + size());
			return newLiteral;
		}

		template<size_type sizeNew> JSONIFIER_INLINE constexpr string_literal<sizeNew + sizeVal - 1> operator+=(const value_type (&str)[sizeNew]) const noexcept {
			string_literal<sizeNew + sizeVal - 1> newLiteral{};
			std::copy(values, values + size(), newLiteral.data());
			std::copy(str, str + sizeNew, newLiteral.data() + size());
			return newLiteral;
		}

		template<size_type sizeNew> JSONIFIER_INLINE constexpr string_literal<sizeNew + sizeVal - 1> operator+(const value_type (&str)[sizeNew]) const noexcept {
			string_literal<sizeNew + sizeVal - 1> newLiteral{};
			std::copy(values, values + size(), newLiteral.data());
			std::copy(str, str + sizeNew, newLiteral.data() + size());
			return newLiteral;
		}

		template<uint64_t offsetNew, uint64_t count, uint64_t newSize = count + 1> JSONIFIER_INLINE constexpr string_literal<newSize> substr() const noexcept {
			static_assert(offsetNew + count <= length);
			string_literal<newSize> returnValue{};
			std::copy(values + offsetNew, values + offsetNew + count, returnValue.values);
			returnValue.values[count] = '\0';
			return returnValue;
		}

		JSONIFIER_INLINE constexpr const_reference operator[](size_type index) const noexcept JSONIFIER_LIFETIME_BOUND {
			return values[index];
		}

		JSONIFIER_INLINE constexpr reference operator[](size_type index) noexcept JSONIFIER_LIFETIME_BOUND {
			return values[index];
		}

		template<typename string_type> JSONIFIER_INLINE constexpr operator string_type() const noexcept {
			string_type returnValues{ values, length };
			return returnValues;
		}

		JSONIFIER_INLINE constexpr const_pointer data() const noexcept JSONIFIER_LIFETIME_BOUND {
			return values;
		}

		JSONIFIER_INLINE constexpr pointer data() noexcept JSONIFIER_LIFETIME_BOUND {
			return values;
		}

		JSONIFIER_INLINE static constexpr size_type size() noexcept {
			return length;
		}

		alignas(simdBytesPerRegister) value_type values[sizeVal]{};
	};

	template<uint64_t sizeVal> string_literal(const char (&)[sizeVal]) -> string_literal<sizeVal>;

	template<uint64_t size, uint64_t bufferSize> inline basic_stream<bufferSize>& operator<<(basic_stream<bufferSize>& os, const string_literal<size>& input) noexcept {
		os << std::basic_string_view<char>{ input.data(), input.size() };
		return os;
	}

	template<uint64_t size> inline std::ostream& operator<<(std::ostream& os, const string_literal<size>& input) noexcept {
		os << std::basic_string_view<char>{ input.data(), input.size() };
		return os;
	}

	template<string_literal literal> consteval uint64_t escapedKeyLength() noexcept {
		uint64_t newLength{};
		for (uint64_t x = 0; x < literal.size(); ++x) {
			const char c = literal[x];
			newLength += (c == '"' || c == '\\') ? 2 : 1;
		}
		return newLength;
	}

	template<string_literal literal> consteval string_literal<escapedKeyLength<literal>() + 1> escapeKeyLiteral() noexcept {
		constexpr uint64_t newLength{ escapedKeyLength<literal>() };
		string_literal<newLength + 1> returnValues{};
		uint64_t outIndex{};
		for (uint64_t x = 0; x < literal.size(); ++x) {
			const char c = literal[x];
			if (c == '"' || c == '\\') {
				returnValues[outIndex] = '\\';
				++outIndex;
			}
			returnValues[outIndex] = c;
			++outIndex;
		}
		returnValues[newLength] = '\0';
		return returnValues;
	}

	template<uint64_t index, typename string_type> consteval string_literal<index + 1> stringLiteralFromView(string_type str) noexcept {
		string_literal<index + 1> sl{};
		std::copy_n(str.data(), str.size(), sl.values);
		sl[index] = '\0';
		return sl;
	}

	template<string_literal literal> inline constexpr auto escapedKeyLiteral{ escapeKeyLiteral<literal>() };

}
