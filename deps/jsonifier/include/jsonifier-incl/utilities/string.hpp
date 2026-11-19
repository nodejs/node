/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/utilities/string.hpp
 */
#pragma once

#include <jsonifier-incl/containers/allocator.hpp>
#include <jsonifier-incl/containers/iterator.hpp>
#include <jsonifier-incl/core/fastio.hpp>
#include <jsonifier-incl/utilities/compare.hpp>

namespace jsonifier {

	class string_view_base;

	template<uint64_t newerSize> class string_base : internal::alloc_wrapper<char> {
	  public:
		using value_type			 = char;
		using pointer				 = value_type*;
		using const_pointer			 = const value_type*;
		using reference				 = value_type&;
		using const_reference		 = const value_type&;
		using difference_type		 = std::ptrdiff_t;
		using iterator				 = internal::basic_iterator<value_type>;
		using const_iterator		 = internal::basic_iterator<const value_type>;
		using reverse_iterator		 = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;
		using size_type				 = uint64_t;
		using allocator				 = internal::alloc_wrapper<value_type>;
		using traits_type			 = std::char_traits<value_type>;
		static constexpr size_type length{ newerSize > 0 ? newerSize - 1 : 0 };
		static constexpr size_type bufferSize = 16 / sizeof(value_type) < 1 ? 1 : 16 / sizeof(value_type);

		static constexpr size_type npos{ std::numeric_limits<size_type>::max() };

		JSONIFIER_INLINE static constexpr size_type fromStdPosition(size_t position) noexcept {
			return position == std::basic_string_view<value_type>::npos ? npos : static_cast<size_type>(position);
		}

		JSONIFIER_INLINE string_base() noexcept : internal::alloc_wrapper<value_type>{}, capacityVal{}, sizeVal{}, dataVal{} {
			if constexpr (newerSize > 0) {
				reserve(newerSize);
			}
		}

		template<internal::pointer_types value_type_newer> JSONIFIER_INLINE string_base(value_type_newer other) noexcept
			: internal::alloc_wrapper<value_type>{}, capacityVal{}, sizeVal{}, dataVal{} {
			if (other) {
				const auto newSize = std::char_traits<std::remove_const_t<internal::remove_pointer_t<value_type_newer>>>::length(other) *
					(sizeof(internal::remove_pointer_t<value_type_newer>) / sizeof(value_type));
				if (newSize > 0 && newSize < maxSize()) [[likely]] {
					reserve(newSize);
					sizeVal = newSize;
					std::uninitialized_copy(other, other + newSize, dataVal);
					allocator::construct(&(*this)[newSize], value_type{});
				}
			}
		}

		template<internal::string_types value_type_newer> JSONIFIER_INLINE string_base(value_type_newer&& other) noexcept
			: internal::alloc_wrapper<value_type>{}, capacityVal{}, sizeVal{}, dataVal{} {
			size_type newSize = other.size() * (sizeof(typename internal::base_t<value_type_newer>::value_type) / sizeof(value_type));
			if (newSize > 0 && newSize < maxSize()) [[likely]] {
				reserve(newSize);
				sizeVal = newSize;
				std::uninitialized_copy(other.data(), other.data() + newSize, dataVal);
				allocator::construct(&(*this)[newSize], value_type{});
			}
		}

		JSONIFIER_INLINE string_base(const_iterator other, uint64_t newSize) noexcept : internal::alloc_wrapper<value_type>{}, capacityVal{}, sizeVal{}, dataVal{} {
			if (newSize > 0 && newSize < maxSize()) [[likely]] {
				reserve(newSize);
				sizeVal = newSize;
				std::uninitialized_copy(other.operator->(), other.operator->() + newSize, dataVal);
				allocator::construct(&(*this)[newSize], value_type{});
			}
		}

		JSONIFIER_INLINE string_base(const_pointer other, uint64_t newSize) noexcept : internal::alloc_wrapper<value_type>{}, capacityVal{}, sizeVal{}, dataVal{} {
			if (newSize > 0 && newSize < maxSize()) [[likely]] {
				reserve(newSize);
				sizeVal = newSize;
				std::uninitialized_copy(other, other + newSize, dataVal);
				allocator::construct(&(*this)[newSize], value_type{});
			}
		}

		template<internal::char_types value_type_newer> JSONIFIER_INLINE string_base(value_type_newer other) noexcept
			: internal::alloc_wrapper<value_type>{}, capacityVal{}, sizeVal{}, dataVal{} {
			*this = other;
		}

		template<uint64_t size>
			requires(size == newerSize && size > 0)
		JSONIFIER_INLINE constexpr string_base(const char (&str)[size]) noexcept {
			resize(newerSize);
			for (uint64_t x = 0; x < length; ++x) {
				dataVal[x] = str[x];
			}
			dataVal[length] = '\0';
		}

		JSONIFIER_INLINE string_base& operator=(string_base&& other) noexcept {
			if (this != &other) [[likely]] {
				string_base newValue{ other };
				swap(newValue);
			}
			return *this;
		}

		JSONIFIER_INLINE explicit string_base(string_base&& other) noexcept : internal::alloc_wrapper<value_type>{}, capacityVal{}, sizeVal{}, dataVal{} {
			swap(other);
		}

		JSONIFIER_INLINE string_base& operator=(const string_base& other) noexcept {
			if (this != &other) [[likely]] {
				string_base newValue{ other };
				swap(newValue);
			}
			return *this;
		}

		JSONIFIER_INLINE string_base(const string_base& other) noexcept : internal::alloc_wrapper<value_type>{}, capacityVal{}, sizeVal{}, dataVal{} {
			size_type newSize = other.size();
			if (newSize > 0 && newSize < maxSize()) [[likely]] {
				reserve(newSize);
				sizeVal = newSize;
				std::uninitialized_copy(other.data(), other.data() + newSize, dataVal);
				allocator::construct(&(*this)[newSize], value_type{});
			}
		}

		template<typename value_type_newer, size_type size> JSONIFIER_INLINE friend string_base operator+=(const value_type_newer (&lhs)[size], const string_base& rhs) noexcept {
			string_base newLhs{ lhs };
			newLhs += rhs;
			return newLhs;
		}

		template<typename value_type_newer, size_type size> JSONIFIER_INLINE friend string_base operator+(const value_type_newer (&lhs)[size], const string_base& rhs) noexcept {
			string_base newLhs{ lhs };
			newLhs += rhs;
			return newLhs;
		}

		template<typename iterator_type01, typename iterator_type02> JSONIFIER_INLINE void insert(iterator_type01 where, iterator_type02 start, iterator_type02 end) noexcept {
			const int64_t rawNewSize = end - start;

			if (rawNewSize <= 0) [[unlikely]] {
				return;
			}

			const auto newSize = static_cast<size_type>(rawNewSize);
			const auto posNew  = static_cast<size_type>(where.operator->() - dataVal);

			if (sizeVal + newSize >= capacityVal) [[unlikely]] {
				reserve(sizeVal + newSize);
			}

			internal::memmoveWrapper(dataVal + posNew + newSize, dataVal + posNew, (sizeVal - posNew) * sizeof(value_type));
			std::copy(start.operator->(), start.operator->() + newSize, dataVal + posNew);
			sizeVal += newSize;
			allocator::construct(&dataVal[sizeVal], value_type{});
		}

		template<internal::pointer_types string_type_new> JSONIFIER_INLINE friend string_base operator+=(string_type_new&& lhs, const string_base& rhs) noexcept {
			string_base newLhs{ lhs };
			newLhs += rhs;
			return newLhs;
		}

		template<internal::string_types value_type_newer> JSONIFIER_INLINE friend bool operator==(const string_base& lhs, const value_type_newer& rhs) noexcept {
			if (lhs.size() == rhs.size()) {
				if (lhs.size() > 0) {
					return internal::comparison::compare(lhs.data(), rhs.data(), rhs.size());
				}
				return true;
			} else {
				return false;
			}
		}

		template<internal::pointer_types string_type_new> JSONIFIER_INLINE friend string_base operator+(string_type_new&& lhs, const string_base& rhs) noexcept {
			string_base newLhs{ lhs };
			newLhs += rhs;
			return newLhs;
		}

		template<typename value_type_newer, size_type size> JSONIFIER_INLINE string_base operator+(const value_type_newer (&rhs)[size]) const noexcept {
			string_base newLhs{ *this };
			newLhs += rhs;
			return newLhs;
		}

		template<typename value_type_newer, size_type size> JSONIFIER_INLINE string_base& operator+=(const value_type_newer (&rhs)[size]) noexcept {
			string_base newRhs{ rhs };
			*this += newRhs;
			return *this;
		}

		template<internal::string_types string_type_new> JSONIFIER_INLINE string_base operator+(const string_type_new& rhs) const noexcept {
			string_base newLhs{ *this };
			newLhs += rhs;
			return newLhs;
		}

		template<internal::string_types string_type_new> JSONIFIER_INLINE string_base& operator+=(const string_type_new& rhs) noexcept {
			append(static_cast<string_base>(rhs));
			return *this;
		}

		template<internal::string_types value_type_newer> JSONIFIER_INLINE string_base& operator=(value_type_newer&& other) noexcept {
			string_base newValue{ other };
			swap(newValue);
			return *this;
		}

		template<internal::pointer_types value_type_newer> JSONIFIER_INLINE string_base& operator=(value_type_newer other) noexcept {
			string_base newValue{ std::forward<value_type_newer>(other) };
			swap(newValue);
			return *this;
		}

		template<internal::pointer_types string_type_new> JSONIFIER_INLINE string_base& operator+=(string_type_new&& rhs) noexcept {
			string_base newRhs{ rhs };
			*this += newRhs;
			return *this;
		}

		template<uint64_t size> JSONIFIER_INLINE friend bool operator==(const string_base& lhs, const char (&rhs)[size]) noexcept {
			auto rhsLength = traits_type::length(rhs);
			return rhsLength == lhs.size() && internal::comparison::compare(lhs.data(), rhs, rhsLength);
		}

		template<internal::pointer_types string_type_new> JSONIFIER_INLINE string_base operator+(string_type_new&& rhs) noexcept {
			string_base newLhs{ *this };
			newLhs += rhs;
			return newLhs;
		}

		template<internal::char_types value_type_newer> JSONIFIER_INLINE string_base& operator=(value_type_newer other) noexcept {
			emplace_back(static_cast<value_type>(other));
			return *this;
		}

		JSONIFIER_INLINE string_base substr(size_type position, size_type count = std::numeric_limits<size_type>::max()) const {
			if (static_cast<int64_t>(position) >= static_cast<int64_t>(sizeVal)) [[unlikely]] {
				JSONIFIER_THROW(std::out_of_range("Substring position is out of range."));
			}

			count = internal::min(count, sizeVal - position);

			string_base result{};
			if (count > 0) [[likely]] {
				result.resize(count);
				std::copy(dataVal + position, dataVal + position + count, result.dataVal);
			}
			return result;
		}

		template<typename value_type_newer> JSONIFIER_INLINE void append(value_type_newer* values, uint64_t newSize) noexcept {
			if (sizeVal + newSize >= capacityVal) [[unlikely]] {
				reserve(sizeVal + newSize);
			}
			if (newSize > 0 && values) [[likely]] {
				std::copy(values, values + newSize, dataVal + sizeVal);
				sizeVal += newSize;
				allocator::construct(&dataVal[sizeVal], value_type{});
			}
		}

		template<typename value_type_newer> JSONIFIER_INLINE explicit operator std::basic_string<value_type_newer>() const {
			std::basic_string<value_type_newer> returnValue{};
			if (sizeVal > 0) [[likely]] {
				returnValue.resize(sizeVal);
				jsonifierMemcpy(returnValue.data(), data(), returnValue.size());
			}
			return returnValue;
		}

		template<typename... arg_types> JSONIFIER_INLINE size_type findFirstNotOf(arg_types&&... args) const noexcept {
			return fromStdPosition(operator std::basic_string_view<value_type>().find_first_not_of(std::forward<arg_types>(args)...));
		}

		template<typename... arg_types> JSONIFIER_INLINE size_type findLastNotOf(arg_types&&... args) const noexcept {
			return fromStdPosition(operator std::basic_string_view<value_type>().find_last_not_of(std::forward<arg_types>(args)...));
		}

		static consteval size_type maxSize() noexcept {
			const size_type allocMax   = allocator::maxSize();
			const size_type storageMax = internal::max(allocMax, static_cast<size_type>(bufferSize));
			return internal::min(static_cast<size_type>((std::numeric_limits<difference_type>::max)()), storageMax - 1);
		}

		template<typename... arg_types> JSONIFIER_INLINE size_type findFirstOf(arg_types&&... args) const noexcept {
			return fromStdPosition(operator std::basic_string_view<value_type>().find_first_of(std::forward<arg_types>(args)...));
		}

		template<typename... arg_types> JSONIFIER_INLINE size_type findLastOf(arg_types&&... args) const noexcept {
			return fromStdPosition(operator std::basic_string_view<value_type>().find_last_of(std::forward<arg_types>(args)...));
		}

		JSONIFIER_INLINE void insert(iterator values, value_type toInsert) noexcept {
			const auto positionNew = static_cast<size_type>(values - begin());
			if (sizeVal + 1 >= capacityVal) [[unlikely]] {
				reserve((sizeVal + 1) * 2);
			}
			const auto newSize = sizeVal - positionNew;
			internal::memmoveWrapper(dataVal + positionNew + 1, dataVal + positionNew, newSize * sizeof(value_type));
			allocator::construct(&dataVal[positionNew], toInsert);
			++sizeVal;
		}

		template<typename... arg_types> JSONIFIER_INLINE size_type rfind(arg_types&&... args) const noexcept {
			return fromStdPosition(operator std::basic_string_view<value_type>().rfind(std::forward<arg_types>(args)...));
		}

		template<typename... arg_types> JSONIFIER_INLINE size_type find(arg_types&&... args) const noexcept {
			return fromStdPosition(operator std::basic_string_view<value_type>().find(std::forward<arg_types>(args)...));
		}

		template<typename string_base_new> JSONIFIER_INLINE void swap(string_base_new&& other) noexcept {
			internal::swap(capacityVal, other.capacityVal);
			internal::swap(sizeVal, other.sizeVal);
			internal::swap(dataVal, other.dataVal);
		}

		JSONIFIER_INLINE const_reference at(size_type index) const {
			if (index >= sizeVal) [[unlikely]] {
				JSONIFIER_THROW(std::runtime_error{ "Sorry, but that index is beyond the end of this string." });
			}
			return dataVal[index];
		}

		JSONIFIER_INLINE reference at(size_type index) {
			if (index >= sizeVal) [[unlikely]] {
				JSONIFIER_THROW(std::runtime_error{ "Sorry, but that index is beyond the end of this string." });
			}
			return dataVal[index];
		}

		JSONIFIER_INLINE operator std::basic_string_view<value_type>() const noexcept {
			return { dataVal, static_cast<size_t>(sizeVal) };
		}

		JSONIFIER_INLINE void append(const string_base& newSize) noexcept {
			if (sizeVal + newSize.size() >= capacityVal) [[unlikely]] {
				reserve(sizeVal + newSize.size());
			}
			if (newSize.size() > 0) [[likely]] {
				std::copy(newSize.data(), newSize.data() + newSize.size(), dataVal + sizeVal);
				sizeVal += newSize.size();
				allocator::construct(&dataVal[sizeVal], value_type{});
			}
		}

		JSONIFIER_INLINE void resize(size_type newSize) {
			if (static_cast<int64_t>(newSize) > 0) [[likely]] {
				if (newSize > capacityVal) [[likely]] {
					pointer newPtr = allocator::allocate(newSize + 1);
					if (dataVal) [[likely]] {
						if (sizeVal > 0) [[likely]] {
							std::uninitialized_move(dataVal, dataVal + sizeVal, newPtr);
						}
						allocator::deallocate(dataVal, capacityVal + 1);
					}
					capacityVal = newSize;
					dataVal		= newPtr;
					std::memset(dataVal + sizeVal, 0, (newSize - sizeVal) * sizeof(value_type));
					allocator::construct(newPtr + newSize, value_type{});
					sizeVal = newSize;
				} else if (newSize > sizeVal) [[likely]] {
					std::memset(dataVal + sizeVal, 0, (newSize - sizeVal) * sizeof(value_type));
					allocator::construct(dataVal + newSize, value_type{});
					sizeVal = newSize;
				} else if (newSize < sizeVal) [[likely]] {
					std::destroy(dataVal + newSize, dataVal + sizeVal);
					allocator::construct(dataVal + newSize, value_type{});
					sizeVal = newSize;
				}
			} else {
				std::destroy(dataVal, dataVal + sizeVal);
				sizeVal = 0;
			}
		}

		JSONIFIER_INLINE const_reference operator[](size_type index) const noexcept {
			return dataVal[index];
		}

		JSONIFIER_INLINE constexpr const_reverse_iterator rbegin() const noexcept {
			return const_reverse_iterator{ end() };
		}

		JSONIFIER_INLINE string_base& operator+=(const value_type& rhs) noexcept {
			emplace_back(rhs);
			return *this;
		}

		JSONIFIER_INLINE constexpr const_reverse_iterator rend() const noexcept {
			return const_reverse_iterator{ begin() };
		}

		JSONIFIER_INLINE void erase(iterator count) noexcept {
			int64_t rawNewSize = count.operator->() - dataVal;
			if (rawNewSize == 0) [[unlikely]] {
				return;
			} else if (rawNewSize > static_cast<int64_t>(sizeVal)) [[unlikely]] {
				rawNewSize = static_cast<int64_t>(sizeVal);
			} else if (rawNewSize < 0) [[unlikely]] {
				return;
			}
			const auto newSize = static_cast<size_type>(rawNewSize);
			traits_type::move(dataVal, dataVal + newSize, sizeVal - newSize);
			sizeVal -= newSize;
			allocator::construct(&dataVal[sizeVal], static_cast<value_type>(0x00u));
		}

		JSONIFIER_INLINE void erase(size_type count) noexcept {
			if (count == 0) [[unlikely]] {
				return;
			} else if (count > sizeVal) [[likely]] {
				count = sizeVal;
			}
			traits_type::move(dataVal, dataVal + count, sizeVal - count);
			sizeVal -= count;
			allocator::construct(&dataVal[sizeVal], static_cast<value_type>(0x00u));
		}

		JSONIFIER_INLINE string_base operator+(const value_type& rhs) noexcept {
			string_base newLhs{ *this };
			newLhs.emplace_back(rhs);
			return newLhs;
		}

		JSONIFIER_INLINE constexpr const_iterator begin() const noexcept {
			return const_iterator{ dataVal };
		}

		JSONIFIER_INLINE reference operator[](size_type index) noexcept {
			return dataVal[index];
		}

		JSONIFIER_INLINE constexpr const_iterator end() const noexcept {
			return const_iterator{ dataVal + sizeVal };
		}

		JSONIFIER_INLINE constexpr size_type capacity() const noexcept {
			return capacityVal;
		}

		JSONIFIER_INLINE void emplace_back(value_type value) noexcept {
			if (sizeVal + 1 >= capacityVal) [[unlikely]] {
				reserve((sizeVal + 2) * 4);
			}
			allocator::construct(&dataVal[sizeVal++], value);
			allocator::construct(&dataVal[sizeVal], value_type{});
		}

		JSONIFIER_INLINE constexpr reverse_iterator rbegin() noexcept {
			return reverse_iterator{ end() };
		}

		JSONIFIER_INLINE constexpr reverse_iterator rend() noexcept {
			return reverse_iterator{ begin() };
		}

		JSONIFIER_INLINE void reserve(size_type capacityNew) {
			if (capacityNew > capacityVal) [[likely]] {
				pointer newPtr = allocator::allocate(capacityNew + 1);
				if (dataVal) [[likely]] {
					if (sizeVal > 0) [[likely]] {
						std::uninitialized_move(dataVal, dataVal + sizeVal, newPtr);
					}
					allocator::deallocate(dataVal, capacityVal + 1);
				}
				capacityVal = capacityNew;
				dataVal		= newPtr;
				if (dataVal) {
					allocator::construct(&dataVal[sizeVal], value_type{});
				}
			}
		}

		JSONIFIER_INLINE constexpr size_type size() const noexcept {
			return sizeVal;
		}

		JSONIFIER_INLINE constexpr bool empty() const noexcept {
			return sizeVal == 0;
		}

		JSONIFIER_INLINE constexpr iterator begin() noexcept {
			return iterator{ dataVal };
		}

		JSONIFIER_INLINE const_pointer data() const noexcept {
			return dataVal;
		}

		JSONIFIER_INLINE constexpr iterator end() noexcept {
			return iterator{ dataVal + sizeVal };
		}

		JSONIFIER_INLINE void clear() noexcept {
			if (sizeVal > 0) [[likely]] {
				allocator::construct(dataVal, value_type{});
			}
			sizeVal = 0;
		}

		JSONIFIER_INLINE pointer data() noexcept {
			return dataVal;
		}

		JSONIFIER_INLINE ~string_base() noexcept {
			reset();
		}

	  protected:
		JSONIFIER_INLINE void reset() noexcept {
			if (dataVal && capacityVal) [[likely]] {
				if (sizeVal) [[likely]] {
					std::destroy(dataVal, dataVal + sizeVal);
					sizeVal = 0;
				}
				allocator::deallocate(dataVal, capacityVal + 1);
				dataVal		= nullptr;
				capacityVal = 0;
			}
		}

		size_type capacityVal{};
		size_type sizeVal{};
		pointer dataVal{};
	};

	template<uint64_t newerSize>
		requires(newerSize > 0 && newerSize <= 16)
	class string_base<newerSize> {
	  public:
		using value_type			 = char;
		using pointer				 = value_type*;
		using const_pointer			 = const value_type*;
		using reference				 = value_type&;
		using const_reference		 = const value_type&;
		using difference_type		 = std::ptrdiff_t;
		using iterator				 = internal::basic_iterator<value_type>;
		using const_iterator		 = internal::basic_iterator<const value_type>;
		using reverse_iterator		 = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;
		using size_type				 = uint64_t;
		using traits_type			 = std::char_traits<value_type>;

		static constexpr size_type length{ newerSize > 0 ? newerSize - 1 : 0 };
		static constexpr size_type bufferSize = 16 / sizeof(value_type) < 1 ? 1 : 16 / sizeof(value_type);
		static constexpr size_type npos{ std::numeric_limits<size_type>::max() };

		JSONIFIER_INLINE static constexpr size_type fromStdPosition(size_t position) noexcept {
			return position == std::basic_string_view<value_type>::npos ? npos : static_cast<size_type>(position);
		}

		JSONIFIER_INLINE constexpr string_base() noexcept : sizeVal{}, dataVal{} {
			if constexpr (newerSize > 0) {
				resize(newerSize);
			}
		}

		JSONIFIER_INLINE constexpr string_base(const char (&str)[newerSize]) noexcept : sizeVal{ length }, dataVal{} {
			for (uint64_t x = 0; x < length; ++x) {
				dataVal[x] = str[x];
			}
			dataVal[length] = value_type{};
		}

		template<internal::string_types value_type_newer> JSONIFIER_INLINE constexpr string_base(value_type_newer&& other) noexcept : sizeVal{}, dataVal{} {
			size_type newSize = other.size() * (sizeof(typename internal::base_t<value_type_newer>::value_type) / sizeof(value_type));
			if (newSize > 0 && newSize < maxSize()) [[likely]] {
				sizeVal = newSize;
				std::copy(other.data(), other.data() + newSize, dataVal);
				dataVal[newSize] = value_type{};
			}
		}

		template<internal::pointer_types value_type_newer> JSONIFIER_INLINE constexpr string_base(value_type_newer other) noexcept : sizeVal{}, dataVal{} {
			if (other) {
				const auto newSize = std::char_traits<std::remove_const_t<internal::remove_pointer_t<value_type_newer>>>::length(other) *
					(sizeof(internal::remove_pointer_t<value_type_newer>) / sizeof(value_type));
				if (newSize > 0 && newSize < maxSize()) [[likely]] {
					sizeVal = newSize;
					std::copy(other, other + newSize, dataVal);
					dataVal[newSize] = value_type{};
				}
			}
		}

		template<internal::char_types value_type_newer> JSONIFIER_INLINE constexpr string_base(value_type_newer other) noexcept : sizeVal{}, dataVal{} {
			*this = other;
		}

		JSONIFIER_INLINE constexpr string_base(const_pointer other, uint64_t newSize) noexcept : sizeVal{}, dataVal{} {
			if (newSize > 0 && newSize < maxSize()) [[likely]] {
				sizeVal = newSize;
				std::copy(other, other + newSize, dataVal);
				dataVal[newSize] = value_type{};
			}
		}

		JSONIFIER_INLINE constexpr string_base(const_iterator other, uint64_t newSize) noexcept : sizeVal{}, dataVal{} {
			if (newSize > 0 && newSize < maxSize()) [[likely]] {
				sizeVal = newSize;
				std::copy(other.operator->(), other.operator->() + newSize, dataVal);
				dataVal[newSize] = value_type{};
			}
		}

		JSONIFIER_INLINE constexpr string_base& operator=(string_base&& other) noexcept {
			if (this != &other) [[likely]] {
				string_base newValue{ other };
				swap(newValue);
			}
			return *this;
		}

		JSONIFIER_INLINE constexpr explicit string_base(string_base&& other) noexcept : sizeVal{}, dataVal{} {
			swap(other);
		}

		JSONIFIER_INLINE constexpr string_base& operator=(const string_base& other) noexcept {
			if (this != &other) [[likely]] {
				string_base newValue{ other };
				swap(newValue);
			}
			return *this;
		}

		JSONIFIER_INLINE constexpr string_base(const string_base& other) noexcept : sizeVal{}, dataVal{} {
			size_type newSize = other.size();
			if (newSize > 0 && newSize < maxSize()) [[likely]] {
				sizeVal = newSize;
				for (size_type x = 0; x < newSize; ++x) {
					dataVal[x] = other.dataVal[x];
				}
				dataVal[newSize] = value_type{};
			}
		}

		template<internal::pointer_types string_type_new> JSONIFIER_INLINE constexpr friend string_base operator+=(string_type_new&& lhs, const string_base& rhs) noexcept {
			string_base newLhs{ lhs };
			newLhs += rhs;
			return newLhs;
		}

		template<internal::string_types value_type_newer> JSONIFIER_INLINE constexpr friend bool operator==(const string_base& lhs, const value_type_newer& rhs) noexcept {
			if (lhs.size() == rhs.size()) {
				if (lhs.size() > 0) {
					return internal::comparison::compare(lhs.data(), rhs.data(), rhs.size());
				}
				return true;
			} else {
				return false;
			}
		}

		template<internal::pointer_types string_type_new> JSONIFIER_INLINE constexpr friend string_base operator+(string_type_new&& lhs, const string_base& rhs) noexcept {
			string_base newLhs{ lhs };
			newLhs += rhs;
			return newLhs;
		}

		template<typename value_type_newer, size_type size> JSONIFIER_INLINE constexpr string_base operator+(const value_type_newer (&rhs)[size]) const noexcept {
			string_base newLhs{ *this };
			newLhs += rhs;
			return newLhs;
		}

		template<typename value_type_newer, size_type size> JSONIFIER_INLINE constexpr string_base& operator+=(const value_type_newer (&rhs)[size]) noexcept {
			string_base newRhs{ rhs };
			*this += newRhs;
			return *this;
		}

		template<internal::string_types string_type_new> JSONIFIER_INLINE constexpr string_base operator+(const string_type_new& rhs) const noexcept {
			string_base newLhs{ *this };
			newLhs += rhs;
			return newLhs;
		}

		template<internal::string_types string_type_new> JSONIFIER_INLINE constexpr string_base& operator+=(const string_type_new& rhs) noexcept {
			append(static_cast<string_base>(rhs));
			return *this;
		}

		template<internal::string_types value_type_newer> JSONIFIER_INLINE constexpr string_base& operator=(value_type_newer&& other) noexcept {
			string_base newValue{ other };
			swap(newValue);
			return *this;
		}

		template<internal::pointer_types value_type_newer> JSONIFIER_INLINE constexpr string_base& operator=(value_type_newer other) noexcept {
			string_base newValue{ std::forward<value_type_newer>(other) };
			swap(newValue);
			return *this;
		}

		template<internal::pointer_types string_type_new> JSONIFIER_INLINE constexpr string_base& operator+=(string_type_new&& rhs) noexcept {
			string_base newRhs{ rhs };
			*this += newRhs;
			return *this;
		}

		template<uint64_t size> JSONIFIER_INLINE constexpr friend bool operator==(const string_base& lhs, const char (&rhs)[size]) noexcept {
			auto rhsLength = traits_type::length(rhs);
			return rhsLength == lhs.size() && internal::comparison::compare(lhs.data(), rhs, rhsLength);
		}

		template<internal::char_types value_type_newer> JSONIFIER_INLINE constexpr string_base& operator=(value_type_newer other) noexcept {
			emplace_back(static_cast<value_type>(other));
			return *this;
		}

		template<internal::pointer_types string_type_new> JSONIFIER_INLINE constexpr string_base operator+(string_type_new&& rhs) noexcept {
			string_base newLhs{ *this };
			newLhs += rhs;
			return newLhs;
		}

		JSONIFIER_INLINE constexpr string_base substr(size_type position, size_type count = std::numeric_limits<size_type>::max()) const {
			if (static_cast<int64_t>(position) >= static_cast<int64_t>(sizeVal)) [[unlikely]] {
				JSONIFIER_THROW(std::out_of_range("Substring position is out of range."));
			}

			count = internal::min(count, sizeVal - position);

			string_base result{};
			if (count > 0) [[likely]] {
				result.sizeVal = count;
				std::copy(dataVal + position, dataVal + position + count, result.dataVal);
				result.dataVal[count] = value_type{};
			}
			return result;
		}

		template<typename value_type_newer> JSONIFIER_INLINE constexpr void append(value_type_newer* values, uint64_t newSize) noexcept {
			if (newSize > 0 && values && sizeVal + newSize < maxSize()) [[likely]] {
				std::copy(values, values + newSize, dataVal + sizeVal);
				sizeVal += newSize;
				dataVal[sizeVal] = value_type{};
			}
		}

		template<typename value_type_newer, size_type size>
		JSONIFIER_INLINE constexpr friend string_base operator+=(const value_type_newer (&lhs)[size], const string_base& rhs) noexcept {
			string_base newLhs{ lhs };
			newLhs += rhs;
			return newLhs;
		}

		template<typename value_type_newer, size_type size>
		JSONIFIER_INLINE constexpr friend string_base operator+(const value_type_newer (&lhs)[size], const string_base& rhs) noexcept {
			string_base newLhs{ lhs };
			newLhs += rhs;
			return newLhs;
		}

		template<typename value_type_newer> JSONIFIER_INLINE explicit constexpr operator std::basic_string<value_type_newer>() const {
			std::basic_string<value_type_newer> returnValue{};
			if (sizeVal > 0) [[likely]] {
				returnValue.resize(sizeVal);
				jsonifierMemcpy(returnValue.data(), data(), returnValue.size());
			}
			return returnValue;
		}

		template<typename... arg_types> JSONIFIER_INLINE constexpr size_type findFirstNotOf(arg_types&&... args) const noexcept {
			return fromStdPosition(operator std::basic_string_view<value_type>().find_first_not_of(std::forward<arg_types>(args)...));
		}

		template<typename... arg_types> JSONIFIER_INLINE constexpr size_type findLastNotOf(arg_types&&... args) const noexcept {
			return fromStdPosition(operator std::basic_string_view<value_type>().find_last_not_of(std::forward<arg_types>(args)...));
		}

		template<typename... arg_types> JSONIFIER_INLINE constexpr size_type findFirstOf(arg_types&&... args) const noexcept {
			return fromStdPosition(operator std::basic_string_view<value_type>().find_first_of(std::forward<arg_types>(args)...));
		}

		template<typename... arg_types> JSONIFIER_INLINE constexpr size_type findLastOf(arg_types&&... args) const noexcept {
			return fromStdPosition(operator std::basic_string_view<value_type>().find_last_of(std::forward<arg_types>(args)...));
		}

		template<typename iterator_type01, typename iterator_type02>
		JSONIFIER_INLINE constexpr void insert(iterator_type01 where, iterator_type02 start, iterator_type02 end) noexcept {
			const int64_t rawNewSize = end - start;

			if (rawNewSize <= 0) [[unlikely]] {
				return;
			}

			const auto newSize = static_cast<size_type>(rawNewSize);
			const auto posNew  = static_cast<size_type>(where.operator->() - dataVal);

			std::copy_backward(dataVal + posNew, dataVal + sizeVal, dataVal + sizeVal + newSize);
			std::copy(start.operator->(), start.operator->() + newSize, dataVal + posNew);
			sizeVal += newSize;
			dataVal[sizeVal] = value_type{};
		}

		template<typename... arg_types> JSONIFIER_INLINE constexpr size_type rfind(arg_types&&... args) const noexcept {
			return fromStdPosition(operator std::basic_string_view<value_type>().rfind(std::forward<arg_types>(args)...));
		}

		template<typename... arg_types> JSONIFIER_INLINE constexpr size_type find(arg_types&&... args) const noexcept {
			return fromStdPosition(operator std::basic_string_view<value_type>().find(std::forward<arg_types>(args)...));
		}

		template<typename string_base_new> JSONIFIER_INLINE constexpr void swap(string_base_new&& other) noexcept {
			for (size_type x = 0; x < bufferSize; ++x) {
				const value_type temp = dataVal[x];
				dataVal[x]			  = other.dataVal[x];
				other.dataVal[x]	  = temp;
			}
			const size_type tempSize = sizeVal;
			sizeVal					 = other.sizeVal;
			other.sizeVal			 = tempSize;
		}

		JSONIFIER_INLINE constexpr operator std::basic_string_view<value_type>() const noexcept {
			return { dataVal, static_cast<size_t>(sizeVal) };
		}

		JSONIFIER_INLINE constexpr void insert(iterator values, value_type toInsert) noexcept {
			const auto positionNew = static_cast<size_type>(values - begin());
			if (sizeVal + 1 >= maxSize()) [[unlikely]] {
				return;
			}
			std::copy_backward(dataVal + positionNew, dataVal + sizeVal, dataVal + sizeVal + 1);
			dataVal[positionNew] = toInsert;
			++sizeVal;
			dataVal[sizeVal] = value_type{};
		}

		JSONIFIER_INLINE constexpr const_reference operator[](size_type index) const noexcept {
			return dataVal[index];
		}

		JSONIFIER_INLINE constexpr const_reference at(size_type index) const {
			if (index >= sizeVal) [[unlikely]] {
				JSONIFIER_THROW(std::runtime_error{ "Sorry, but that index is beyond the end of this string." });
			}
			return dataVal[index];
		}

		JSONIFIER_INLINE constexpr reference at(size_type index) {
			if (index >= sizeVal) [[unlikely]] {
				JSONIFIER_THROW(std::runtime_error{ "Sorry, but that index is beyond the end of this string." });
			}
			return dataVal[index];
		}

		JSONIFIER_INLINE constexpr string_base& operator+=(const value_type& rhs) noexcept {
			emplace_back(rhs);
			return *this;
		}

		JSONIFIER_INLINE constexpr string_base operator+(const value_type& rhs) noexcept {
			string_base newLhs{ *this };
			newLhs.emplace_back(rhs);
			return newLhs;
		}

		JSONIFIER_INLINE constexpr const_reverse_iterator rbegin() const noexcept {
			return const_reverse_iterator{ end() };
		}

		JSONIFIER_INLINE constexpr void append(const string_base& other) noexcept {
			const size_type newSize = other.size();
			if (newSize > 0 && sizeVal + newSize < maxSize()) [[likely]] {
				std::copy(other.data(), other.data() + newSize, dataVal + sizeVal);
				sizeVal += newSize;
				dataVal[sizeVal] = value_type{};
			}
		}

		JSONIFIER_INLINE constexpr reference operator[](size_type index) noexcept {
			return dataVal[index];
		}

		JSONIFIER_INLINE constexpr const_reverse_iterator rend() const noexcept {
			return const_reverse_iterator{ begin() };
		}

		JSONIFIER_INLINE constexpr void emplace_back(value_type value) noexcept {
			if (sizeVal + 1 >= maxSize()) [[unlikely]] {
				return;
			}
			dataVal[sizeVal++] = value;
			dataVal[sizeVal]   = value_type{};
		}

		JSONIFIER_INLINE constexpr void erase(iterator count) noexcept {
			int64_t rawNewSize = count.operator->() - dataVal;
			if (rawNewSize == 0) [[unlikely]] {
				return;
			} else if (rawNewSize > static_cast<int64_t>(sizeVal)) [[unlikely]] {
				rawNewSize = static_cast<int64_t>(sizeVal);
			} else if (rawNewSize < 0) [[unlikely]] {
				return;
			}
			const auto newSize = static_cast<size_type>(rawNewSize);
			traits_type::move(dataVal, dataVal + newSize, sizeVal - newSize);
			sizeVal -= newSize;
			dataVal[sizeVal] = static_cast<value_type>(0x00u);
		}

		JSONIFIER_INLINE constexpr void resize(size_type newSize) noexcept {
			if (newSize < maxSize()) [[likely]] {
				if (newSize > sizeVal) {
					for (size_type x = sizeVal; x < newSize; ++x) {
						dataVal[x] = value_type{};
					}
				}
				sizeVal			 = newSize;
				dataVal[sizeVal] = value_type{};
			}
		}

		JSONIFIER_INLINE constexpr const_iterator begin() const noexcept {
			return const_iterator{ dataVal };
		}

		JSONIFIER_INLINE constexpr void erase(size_type count) noexcept {
			if (count == 0) [[unlikely]] {
				return;
			} else if (count > sizeVal) [[likely]] {
				count = sizeVal;
			}
			traits_type::move(dataVal, dataVal + count, sizeVal - count);
			sizeVal -= count;
			dataVal[sizeVal] = static_cast<value_type>(0x00u);
		}

		JSONIFIER_INLINE constexpr const_iterator end() const noexcept {
			return const_iterator{ dataVal + sizeVal };
		}

		JSONIFIER_INLINE constexpr size_type capacity() const noexcept {
			return bufferSize;
		}

		JSONIFIER_INLINE constexpr const_pointer data() const noexcept {
			return dataVal;
		}

		JSONIFIER_INLINE constexpr reverse_iterator rbegin() noexcept {
			return reverse_iterator{ end() };
		}

		JSONIFIER_INLINE constexpr reverse_iterator rend() noexcept {
			return reverse_iterator{ begin() };
		}

		JSONIFIER_INLINE constexpr size_type size() const noexcept {
			return sizeVal;
		}

		JSONIFIER_INLINE constexpr bool empty() const noexcept {
			return sizeVal == 0;
		}

		JSONIFIER_INLINE constexpr iterator begin() noexcept {
			return iterator{ dataVal };
		}

		JSONIFIER_INLINE constexpr iterator end() noexcept {
			return iterator{ dataVal + sizeVal };
		}

		JSONIFIER_INLINE constexpr pointer data() noexcept {
			return dataVal;
		}

		JSONIFIER_INLINE constexpr void clear() noexcept {
			dataVal[0] = value_type{};
			sizeVal	   = 0;
		}

		static consteval size_type maxSize() noexcept {
			return bufferSize;
		}

		JSONIFIER_INLINE constexpr ~string_base() noexcept = default;

	  protected:
		value_type dataVal[bufferSize + 1]{};
		size_type sizeVal{};
	};

	using string = string_base<17>;

	template<uint64_t size> string_base(const char (&)[size]) -> string_base<size>;

	template<uint64_t size, uint64_t bufferSize>
	JSONIFIER_INLINE internal::basic_stream<bufferSize>& operator<<(internal::basic_stream<bufferSize>& os, const string_base<size>& input) noexcept {
		os << std::basic_string_view<char>{ input.data(), input.size() };
		return os;
	}

	template<uint64_t size> JSONIFIER_INLINE std::ostream& operator<<(std::ostream& os, const string_base<size>& input) noexcept {
		os << std::basic_string_view<char>{ input.data(), input.size() };
		return os;
	}
}
