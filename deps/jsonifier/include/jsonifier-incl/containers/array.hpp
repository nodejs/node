/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/containers/array.hpp
 */
#pragma once

#include <jsonifier-incl/core/config.hpp>
#include <jsonifier-incl/containers/iterator.hpp>

namespace jsonifier::internal {

	template<typename value_type_new, uint64_t sizeNew> struct array {
	  public:
		static_assert(std::is_object_v<value_type_new>, "The C++ Standard forbids containers of non-object types because of [container.requirements].");

		using value_type		= value_type_new;
		using size_type			= uint64_t;
		using difference_type	= ptrdiff_t;
		using reference			= value_type&;
		using r_reference		= value_type&&;
		using const_reference	= const value_type&;
		using const_r_reference = const value_type&&;
		using pointer			= value_type*;
		using const_pointer		= const value_type*;
		using iterator			= sized_iterator<value_type, sizeNew>;
		using const_iterator	= sized_iterator<const value_type, sizeNew>;

		template<indexable_types<size_type> index_type> JSONIFIER_INLINE constexpr const_r_reference operator[](index_type position) const&& noexcept {
			return static_cast<const_r_reference>(dataVal[static_cast<uint64_t>(position)]);
		}

		template<indexable_types<size_type> index_type> JSONIFIER_INLINE constexpr const_reference operator[](index_type position) const& noexcept {
			return static_cast<const_reference>(dataVal[static_cast<uint64_t>(position)]);
		}

		template<integral_constant_types tag_type> JSONIFIER_INLINE constexpr const_r_reference operator[](tag_type position) const&& noexcept {
			static_assert(static_cast<uint64_t>(position) < sizeNew, "Sorry, but that index is out of bounds!");
			return static_cast<const_r_reference>(dataVal[static_cast<uint64_t>(position)]);
		}

		template<integral_constant_types tag_type> JSONIFIER_INLINE constexpr const_reference operator[](tag_type position) const& noexcept {
			static_assert(static_cast<uint64_t>(position) < sizeNew, "Sorry, but that index is out of bounds!");
			return static_cast<const_reference>(dataVal[static_cast<uint64_t>(position)]);
		}

		template<indexable_types<size_type> index_type> JSONIFIER_INLINE constexpr r_reference operator[](index_type position) && noexcept {
			return static_cast<r_reference>(dataVal[static_cast<uint64_t>(position)]);
		}

		template<indexable_types<size_type> index_type> JSONIFIER_INLINE constexpr reference operator[](index_type position) & noexcept {
			return static_cast<reference>(dataVal[static_cast<uint64_t>(position)]);
		}

		template<integral_constant_types tag_type> JSONIFIER_INLINE constexpr r_reference operator[](tag_type position) && noexcept {
			static_assert(static_cast<uint64_t>(position) < sizeNew, "Sorry, but that index is out of bounds!");
			return static_cast<r_reference>(dataVal[static_cast<uint64_t>(position)]);
		}

		template<integral_constant_types tag_type> JSONIFIER_INLINE constexpr reference operator[](tag_type position) & noexcept {
			static_assert(static_cast<uint64_t>(position) < sizeNew, "Sorry, but that index is out of bounds!");
			return static_cast<reference>(dataVal[static_cast<uint64_t>(position)]);
		}

		JSONIFIER_INLINE constexpr void swap(array& other) noexcept(std::is_nothrow_swappable<value_type>::value) {
			std::swap_ranges(dataVal, dataVal + sizeNew, other.dataVal);
		}

		JSONIFIER_INLINE constexpr friend bool operator==(const array& lhs, const array& rhs) noexcept {
			for (uint64_t x = 0; x < sizeNew; ++x) {
				if (lhs[x] != rhs[x]) {
					return false;
				}
			}
			return true;
		}

		JSONIFIER_INLINE constexpr friend bool operator!=(const array& lhs, const array& rhs) noexcept {
			return !(lhs == rhs);
		}

		JSONIFIER_INLINE constexpr const_reference front() const noexcept JSONIFIER_LIFETIME_BOUND {
			return dataVal[0];
		}

		JSONIFIER_INLINE constexpr const_iterator begin() const noexcept JSONIFIER_LIFETIME_BOUND {
			return const_iterator{ dataVal };
		}

		JSONIFIER_INLINE constexpr const_reference back() const noexcept JSONIFIER_LIFETIME_BOUND {
			return dataVal[sizeNew - 1];
		}

		JSONIFIER_INLINE constexpr const_iterator end() const noexcept JSONIFIER_LIFETIME_BOUND {
			return const_iterator{ dataVal + sizeNew };
		}

		JSONIFIER_INLINE constexpr reference front() noexcept JSONIFIER_LIFETIME_BOUND {
			return dataVal[0];
		}

		JSONIFIER_INLINE constexpr iterator begin() noexcept JSONIFIER_LIFETIME_BOUND {
			return iterator{ dataVal };
		}

		JSONIFIER_INLINE constexpr reference back() noexcept JSONIFIER_LIFETIME_BOUND {
			return dataVal[sizeNew - 1];
		}

		JSONIFIER_INLINE constexpr iterator end() noexcept JSONIFIER_LIFETIME_BOUND {
			return iterator{ dataVal + sizeNew };
		}

		JSONIFIER_INLINE constexpr void fill(const value_type& value) noexcept {
			std::fill_n(dataVal, sizeNew, value);
		}

		JSONIFIER_INLINE constexpr const_pointer data() const noexcept {
			return dataVal;
		}

		JSONIFIER_INLINE constexpr size_type size() const noexcept {
			return sizeNew;
		}

		JSONIFIER_INLINE constexpr bool empty() const noexcept {
			return false;
		}

		JSONIFIER_INLINE constexpr pointer data() noexcept {
			return dataVal;
		}

		value_type dataVal[sizeNew];
	};

	template<typename first, typename... rest> array(first, rest...) -> array<base_t<first>, 1 + sizeof...(rest)>;

	template<typename first, uint64_t size> array(array<first, size>) -> array<base_t<first>, size>;

	template<typename value_type_new> struct array<value_type_new, 0ULL> {
	  public:
		using value_type	  = value_type_new;
		using size_type		  = uint64_t;
		using difference_type = ptrdiff_t;
		using reference		  = value_type&;
		using const_reference = const value_type&;
		using pointer		  = value_type*;
		using const_pointer	  = const value_type*;
		using iterator		  = sized_iterator<value_type, 0ULL>;
		using const_iterator  = sized_iterator<const value_type, 0ULL>;

		JSONIFIER_INLINE constexpr friend bool operator==(const array&, const array&) noexcept {
			return true;
		}

		JSONIFIER_INLINE constexpr friend bool operator!=(const array&, const array&) noexcept {
			return false;
		}

		JSONIFIER_INLINE constexpr size_type size() const noexcept {
			return 0;
		}

		JSONIFIER_INLINE constexpr bool empty() const noexcept {
			return true;
		}
	};

}
