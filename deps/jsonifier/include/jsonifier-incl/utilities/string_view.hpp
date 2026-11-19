/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/utilities/string_view.hpp
 */
#pragma once

#include <jsonifier-incl/core/fastio.hpp>
#include <jsonifier-incl/utilities/string.hpp>

namespace jsonifier {

	class string_view_base {
	  public:
		using value_type			 = char;
		using const_pointer			 = const value_type*;
		using const_reference		 = const value_type&;
		using iterator				 = internal::basic_iterator<value_type>;
		using const_iterator		 = internal::basic_iterator<const value_type>;
		using difference_type		 = std::ptrdiff_t;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;
		using size_type				 = uint64_t;
		using traits_type			 = std::char_traits<value_type>;
		static constexpr size_type npos{ std::numeric_limits<size_type>::max() };

		JSONIFIER_INLINE static constexpr size_type fromStdPosition(size_t position) noexcept {
			return position == std::basic_string_view<value_type>::npos ? npos : static_cast<size_type>(position);
		}

		JSONIFIER_INLINE constexpr string_view_base() noexcept : dataVal{}, sizeVal{ 0 } {
		}

		template<typename value_type_newer, internal::same_character_size_types<value_type>>
		JSONIFIER_INLINE constexpr string_view_base(const value_type_newer& stringNew) noexcept {
			*this = stringNew;
		}

		JSONIFIER_INLINE constexpr string_view_base(const_pointer pointerNew) noexcept : dataVal(pointerNew), sizeVal(std::char_traits<value_type>::length(pointerNew)) {
		}

		template<internal::string_types value_type_newer> JSONIFIER_INLINE constexpr string_view_base(value_type_newer&& stringNew) noexcept {
			*this = stringNew;
		}

		JSONIFIER_INLINE constexpr string_view_base(const_pointer pointerNew, const size_type countNew) noexcept : dataVal(pointerNew), sizeVal(countNew) {
		}

		template<uint64_t size> JSONIFIER_INLINE constexpr string_view_base(const string_base<size>& stringNew) noexcept {
			*this = stringNew;
		}

		template<internal::string_types value_type_newer> JSONIFIER_INLINE friend bool operator==(const string_view_base& lhs, const value_type_newer& rhs) noexcept {
			if (lhs.size() == rhs.size()) {
				if (lhs.size() > 0) {
					return internal::comparison::compare(lhs.data(), rhs.data(), rhs.size());
				}
				return true;
			} else {
				return false;
			}
		}

		template<typename value_type_newer = value_type> JSONIFIER_INLINE constexpr explicit operator std::basic_string_view<value_type_newer>() const noexcept {
			return { data(), static_cast<size_t>(size()) };
		}

		template<internal::string_types value_type_newer> JSONIFIER_INLINE constexpr string_view_base& operator=(value_type_newer&& stringNew) noexcept {
			dataVal = stringNew.data();
			sizeVal = stringNew.size();
			return *this;
		}

		template<typename value_type_newer = value_type> JSONIFIER_INLINE constexpr explicit operator std::basic_string<value_type_newer>() const {
			std::basic_string<value_type_newer> returnValue{};
			returnValue.resize(sizeVal);
			if (sizeVal > 0 && dataVal) [[likely]] {
				internal::jsonifierMemcpy(returnValue.data(), data(), returnValue.size());
			}
			return returnValue;
		}

		JSONIFIER_INLINE constexpr size_type maxSize() const noexcept {
			return internal::min(static_cast<uint64_t>(std::numeric_limits<std::ptrdiff_t>::max()), static_cast<uint64_t>(-1) / sizeof(value_type));
		}

		template<uint64_t size> JSONIFIER_INLINE friend bool operator==(const string_view_base& lhs, const char (&rhs)[size]) noexcept {
			auto rhsLength = traits_type::length(rhs);
			return rhsLength == lhs.size() && internal::comparison::compare(lhs.data(), rhs, rhsLength);
		}

		template<uint64_t size> JSONIFIER_INLINE constexpr string_view_base& operator=(const string_base<size>& stringNew) noexcept {
			dataVal = stringNew.data();
			sizeVal = stringNew.size();
			return *this;
		}

		template<typename... arg_types> JSONIFIER_INLINE constexpr size_type findFirstNotOf(arg_types&&... args) const noexcept {
			return fromStdPosition(this->operator std::basic_string_view<value_type>().find_first_not_of(std::forward<arg_types>(args)...));
		}

		template<typename... arg_types> JSONIFIER_INLINE constexpr size_type findLastNotOf(arg_types&&... args) const noexcept {
			return fromStdPosition(this->operator std::basic_string_view<value_type>().find_last_not_of(std::forward<arg_types>(args)...));
		}

		template<typename... arg_types> JSONIFIER_INLINE constexpr size_type findFirstOf(arg_types&&... args) const noexcept {
			return fromStdPosition(this->operator std::basic_string_view<value_type>().find_first_of(std::forward<arg_types>(args)...));
		}

		template<typename... arg_types> JSONIFIER_INLINE constexpr size_type findLastOf(arg_types&&... args) const noexcept {
			return fromStdPosition(this->operator std::basic_string_view<value_type>().find_last_of(std::forward<arg_types>(args)...));
		}

		JSONIFIER_INLINE constexpr string_view_base substr(const size_type offsetNew = 0, size_type countNew = npos) const {
			if (offsetNew > sizeVal) [[unlikely]] {
				JSONIFIER_THROW(std::out_of_range("Substring position is out of range."));
			}

			countNew = internal::min(countNew, sizeVal - offsetNew);
			return string_view_base(dataVal + offsetNew, countNew);
		}

		template<typename... arg_types> JSONIFIER_INLINE constexpr size_type rfind(arg_types&&... args) const noexcept {
			return fromStdPosition(this->operator std::basic_string_view<value_type>().rfind(std::forward<arg_types>(args)...));
		}

		template<typename... arg_types> JSONIFIER_INLINE constexpr size_type find(arg_types&&... args) const noexcept {
			return fromStdPosition(this->operator std::basic_string_view<value_type>().find(std::forward<arg_types>(args)...));
		}

		template<uint64_t size> JSONIFIER_INLINE constexpr explicit operator string_base<size>() const noexcept {
			string_base<size> returnValue{};
			returnValue.resize(sizeVal);
			if (sizeVal > 0 && dataVal) [[likely]] {
				jsonifierMemcpy(returnValue.data(), data(), returnValue.size());
			}
			return returnValue;
		}

		template<typename value_type_newer, internal::same_character_size_types<value_type>>
		JSONIFIER_INLINE constexpr string_view_base& operator=(const value_type_newer& stringNew) noexcept {
			dataVal = stringNew.data();
			sizeVal = stringNew.size();
			return *this;
		}

		JSONIFIER_INLINE constexpr const_reference at(const size_type offsetNew) const noexcept(false) {
			if (offsetNew >= sizeVal) [[unlikely]] {
				JSONIFIER_THROW(std::out_of_range{ "Sorry, but that index is beyond the end of this string_view instance." });
			}
			return dataVal[offsetNew];
		}

		JSONIFIER_INLINE constexpr const_reference operator[](const size_type offsetNew) const noexcept {
			return dataVal[offsetNew];
		}

		JSONIFIER_INLINE constexpr const_reverse_iterator rbegin() const noexcept {
			return const_reverse_iterator{ end() };
		}

		JSONIFIER_INLINE constexpr const_reverse_iterator rend() const noexcept {
			return const_reverse_iterator{ begin() };
		}

		JSONIFIER_INLINE constexpr void swap(string_view_base& other) noexcept {
			internal::swap(dataVal, other.dataVal);
			internal::swap(sizeVal, other.sizeVal);
		}

		JSONIFIER_INLINE constexpr const_iterator begin() const noexcept {
			return const_iterator{ dataVal };
		}

		JSONIFIER_INLINE constexpr const_iterator end() const noexcept {
			return const_iterator{ dataVal + sizeVal };
		}

		JSONIFIER_INLINE constexpr const_pointer data() const noexcept {
			return dataVal;
		}

		JSONIFIER_INLINE constexpr size_type length() const noexcept {
			return sizeVal;
		}

		JSONIFIER_INLINE constexpr size_type size() const noexcept {
			return sizeVal;
		}

		JSONIFIER_INLINE constexpr bool empty() const noexcept {
			return sizeVal == 0;
		}

		const_pointer dataVal{};
		size_type sizeVal{};
	};

	using string_view = string_view_base;

	template<uint64_t bufferSize> JSONIFIER_INLINE internal::basic_stream<bufferSize>& operator<<(internal::basic_stream<bufferSize>& os, const string_view& input) noexcept {
		os << std::basic_string_view<char>{ input.data(), static_cast<size_t>(input.size()) };
		return os;
	}

	JSONIFIER_INLINE std::ostream& operator<<(std::ostream& os, const string_view& input) noexcept {
		os << std::basic_string_view<char>{ input.data(), static_cast<size_t>(input.size()) };
		return os;
	}

	JSONIFIER_INLINE constexpr string_view operator""_sv(const char* stringNew, size_t lengthNew) noexcept {
		return string_view(stringNew, lengthNew);
	}

}
