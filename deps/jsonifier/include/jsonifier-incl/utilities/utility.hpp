/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/utilities/utility.hpp
 */
#pragma once

#include <jsonifier-incl/containers/array.hpp>
#include <jsonifier-incl/utilities/concepts.hpp>

namespace jsonifier::internal {

	template<char valueNew> struct uint_c {
		static constexpr uint8_t value{ static_cast<uint8_t>(valueNew) };
	};

	template<char valueNew> inline constexpr uint8_t uint_c_v{ uint_c<valueNew>::value };

	template<typename value_type> using base_t = remove_cvref_t<value_type>;

	consteval array<bool, 256ULL> genWhitespaceTable() noexcept {
		array<bool, 256ULL> returnValues{};
		returnValues[uint_c_v<'\t'>] = true;
		returnValues[uint_c_v<' '>]	 = true;
		returnValues[uint_c_v<'\n'>] = true;
		returnValues[uint_c_v<'\r'>] = true;
		return returnValues;
	}

	alignas(64) inline constexpr auto whitespaceTableRaw{ genWhitespaceTable() };

	inline constexpr const bool* __restrict whitespaceTable{ whitespaceTableRaw.data() };

	consteval array<int8_t, 256ULL> genNestingDeltaTable() noexcept {
		array<int8_t, 256ULL> returnValues{};
		returnValues[uint_c_v<'{'>] = 1;
		returnValues[uint_c_v<'['>] = 1;
		returnValues[uint_c_v<'}'>] = -1;
		returnValues[uint_c_v<']'>] = -1;
		return returnValues;
	}

	alignas(64) inline constexpr auto nestingDeltaTableRaw{ genNestingDeltaTable() };

	inline constexpr const int8_t* __restrict nestingDeltaTable{ nestingDeltaTableRaw.data() };

	consteval array<bool, 256ULL> genNewlineTable() noexcept {
		array<bool, 256ULL> returnValues{};
		returnValues[uint_c_v<'\n'>] = true;
		returnValues[uint_c_v<'\r'>] = true;
		return returnValues;
	}

	alignas(64) inline constexpr auto newlineTableRaw{ genNewlineTable() };

	inline constexpr const bool* __restrict newlineTable{ newlineTableRaw.data() };

	consteval array<bool, 256ULL> genNumberTable() noexcept {
		array<bool, 256ULL> returnValues{};
		returnValues[uint_c_v<'-'>] = true;
		returnValues[uint_c_v<'0'>] = true;
		returnValues[uint_c_v<'1'>] = true;
		returnValues[uint_c_v<'2'>] = true;
		returnValues[uint_c_v<'3'>] = true;
		returnValues[uint_c_v<'4'>] = true;
		returnValues[uint_c_v<'5'>] = true;
		returnValues[uint_c_v<'6'>] = true;
		returnValues[uint_c_v<'7'>] = true;
		returnValues[uint_c_v<'8'>] = true;
		returnValues[uint_c_v<'9'>] = true;
		return returnValues;
	}

	alignas(64) inline constexpr auto numberTableRaw{ genNumberTable() };

	inline constexpr const bool* __restrict numberTable{ numberTableRaw.data() };

	struct alignas(8) char_escape_entry {
		uint64_t size{};
		char values[8]{};
	};

	consteval array<char_escape_entry, 256ULL> genCharEscapeEntries() noexcept {
		constexpr char hexDigits[]{ "0123456789ABCDEF" };
		array<char_escape_entry, 256ULL> returnValue{};
		for (uint64_t x = 0; x < 256ULL; ++x) {
			if (x < 0x20) {
				returnValue[x].values[0] = '\\';
				returnValue[x].values[1] = 'u';
				returnValue[x].values[2] = '0';
				returnValue[x].values[3] = '0';
				returnValue[x].values[4] = hexDigits[(x >> 4) & 0xF];
				returnValue[x].values[5] = hexDigits[x & 0xF];
				returnValue[x].size		 = 6;
			} else if (x < 0x80) {
				returnValue[x].values[0] = static_cast<char>(x);
				returnValue[x].size		 = 1;
			} else {
				returnValue[x].values[0] = static_cast<char>(0xEF);
				returnValue[x].values[1] = static_cast<char>(0xBF);
				returnValue[x].values[2] = static_cast<char>(0xBD);
				returnValue[x].size		 = 3;
			}
		}
		auto setEscape = [&](char value, char escaped) {
			auto& entry		= returnValue[static_cast<uint8_t>(value)];
			entry			= char_escape_entry{};
			entry.values[0] = '\\';
			entry.values[1] = escaped;
			entry.size		= 2;
		};
		setEscape('\b', 'b');
		setEscape('\t', 't');
		setEscape('\n', 'n');
		setEscape('\f', 'f');
		setEscape('\r', 'r');
		setEscape('\"', '\"');
		setEscape('\\', '\\');
		return returnValue;
	}

	alignas(64) inline constexpr auto charEscapeEntriesRaw{ genCharEscapeEntries() };

	inline constexpr const char_escape_entry* charEscapeEntries{ charEscapeEntriesRaw.data() };

	template<typename value_type> JSONIFIER_INLINE constexpr value_type&& forward(remove_reference_t<value_type>& t JSONIFIER_LIFETIME_BOUND) noexcept {
		return static_cast<value_type&&>(t);
	}

	template<typename value_type> JSONIFIER_INLINE constexpr remove_reference_t<value_type>&& move(value_type&& value) noexcept {
		return static_cast<remove_reference_t<value_type>&&>(value);
	}

	template<typename value_type>
		requires(std::is_rvalue_reference_v<value_type>)
	JSONIFIER_INLINE constexpr value_type&& forward(remove_reference_t<value_type>&& t) noexcept {
		static_assert(!std::is_lvalue_reference_v<value_type>, "value_type cannot be an lvalue reference (e.g., U&).");
		return static_cast<value_type&&>(t);
	}

	struct swap_fn {
		template<typename value_type, std::size_t size> JSONIFIER_INLINE constexpr void operator()(value_type (&lhs)[size], value_type (&rhs)[size]) const
			noexcept(noexcept((*this)(lhs[0], rhs[0]))) {
			for (std::size_t index = 0; index < size; ++index) {
				(*this)(lhs[index], rhs[index]);
			}
		}

		template<typename value_type>
			requires(std::is_move_constructible_v<value_type> && std::is_move_assignable_v<value_type>)
		JSONIFIER_INLINE constexpr void operator()(value_type& lhs, value_type& rhs) const
			noexcept(std::is_nothrow_move_constructible_v<value_type> && std::is_nothrow_move_assignable_v<value_type>) {
			value_type temp{ internal::move(lhs) };
			lhs = internal::move(rhs);
			rhs = internal::move(temp);
		}
	};

	inline constexpr swap_fn swap{};

	template<uint64_t bytesProcessedNew, typename simd_type, typename integer_type_new, integer_type_new maskNew> struct type_holder {
		static constexpr uint64_t bytesProcessed{ bytesProcessedNew };
		static constexpr integer_type_new mask{ maskNew };
		using type		   = simd_type;
		using integer_type = integer_type_new;
	};

	template<typename value_type> struct get_int_type {
		using type = conditional_t<std::is_unsigned_v<value_type>, uint8_t, int8_t>;
	};

	template<uint64_t valueNew> struct integral_constant {
		static constexpr uint64_t value = valueNew;

		JSONIFIER_INLINE constexpr uint64_t operator()() const noexcept {
			return value;
		}

		JSONIFIER_INLINE constexpr operator uint64_t() const noexcept {
			return value;
		}
	};

	template<uint64_t index> using tag = integral_constant<index>;

	template<uint64_t... indices> struct integer_sequence {};

	template<typename sequence_one, typename sequence_two> struct concat_sequence;

	template<uint64_t... firstValues, uint64_t... secondValues> struct concat_sequence<integer_sequence<firstValues...>, integer_sequence<secondValues...>> {
		using type = integer_sequence<firstValues..., (secondValues + sizeof...(firstValues))...>;
	};

	template<uint64_t count> struct make_sequence_impl {
		using half_type		 = typename make_sequence_impl<count / 2>::type;
		using remainder_type = typename make_sequence_impl<count - count / 2>::type;
		using type			 = typename concat_sequence<half_type, remainder_type>::type;
	};

	template<> struct make_sequence_impl<0ULL> {
		using type = integer_sequence<>;
	};

	template<> struct make_sequence_impl<1ULL> {
		using type = integer_sequence<0>;
	};

	template<uint64_t count> using make_integer_sequence = typename make_sequence_impl<count>::type;

	template<typename integer_sequence, uint64_t offset> struct offset_sequence;

	template<uint64_t... indices, uint64_t offset> struct offset_sequence<integer_sequence<indices...>, offset> {
		using type = integer_sequence<indices + offset...>;
	};

	template<typename integer_sequence, uint64_t step> struct step_sequence;

	template<uint64_t... indices, uint64_t stepNew> struct step_sequence<integer_sequence<indices...>, stepNew> {
		using type = integer_sequence<indices * stepNew...>;
	};

	template<typename integer_sequence, uint64_t step> using step_sequence_t = typename step_sequence<integer_sequence, step>::type;

	template<uint64_t start, uint64_t end, uint64_t step>
		requires(end >= start && step > 0)
	using make_stepped_range_sequence = typename offset_sequence<step_sequence_t<make_integer_sequence<(end - start + step - 1) / step>, step>, start>::type;

	template<template<auto...> typename functor_type, typename integer_sequence, auto...> struct functor_runner;

	template<template<auto...> typename functor_type, uint64_t... indices, auto... values> struct functor_runner<functor_type, integer_sequence<indices...>, values...> {
		template<typename... arg_types> JSONIFIER_INLINE static auto implAnd([[maybe_unused]] arg_types&&... args) noexcept {
			return (functor_type<values...>::template impl<indices>(internal::forward<arg_types>(args)...) && ...);
		}

		template<typename... arg_types> JSONIFIER_INLINE static auto impl([[maybe_unused]] arg_types&&... args) noexcept {
			return (functor_type<values...>::template impl<indices>(internal::forward<arg_types>(args)...), ...);
		}
	};

	template<template<auto...> typename functor_type, uint64_t... indices, uint64_t offsetVal, auto... values>
	struct functor_runner<functor_type, offset_sequence<integer_sequence<indices...>, offsetVal>, values...> {
		template<typename... arg_types> JSONIFIER_INLINE static auto implAnd([[maybe_unused]] arg_types&&... args) noexcept {
			return (functor_type<values...>::template impl<indices + offsetVal>(internal::forward<arg_types>(args)...) && ...);
		}

		template<typename... arg_types> JSONIFIER_INLINE static auto impl([[maybe_unused]] arg_types&&... args) noexcept {
			return (functor_type<values...>::template impl<indices + offsetVal>(internal::forward<arg_types>(args)...), ...);
		}
	};

	template<typename function_type, typename sequence_type> struct visit_impl;

	template<typename function_type, uint64_t... indices> struct visit_impl<function_type, integer_sequence<indices...>> {
		template<typename variant_type, typename... arg_types> JSONIFIER_INLINE static constexpr void impl(variant_type&& variant, arg_types&&... args) noexcept {
			const auto idx = variant.index();
			static_cast<void>((
				(idx == indices ? (function_type::impl(std::get<indices>(internal::forward<variant_type>(variant)), internal::forward<arg_types>(args)...), true) : false) || ...));
		}
	};

	template<typename function_type, typename variant_type, typename... arg_types> JSONIFIER_INLINE constexpr void visit(variant_type&& variant, arg_types&&... args) noexcept {
		visit_impl<function_type, make_integer_sequence<std::variant_size_v<base_t<variant_type>>>>::impl(internal::forward<variant_type>(variant),
			internal::forward<arg_types>(args)...);
	}

	template<integral_types value_type01, integral_types value_type02> JSONIFIER_INLINE constexpr value_type01 max(value_type01 value1, value_type02 value2) noexcept {
		return value1 > static_cast<value_type01>(value2) ? value1 : static_cast<value_type01>(value2);
	}

	template<integral_types value_type01, integral_types value_type02> JSONIFIER_INLINE constexpr value_type01 min(value_type01 value1, value_type02 value2) noexcept {
		return value1 < static_cast<value_type01>(value2) ? value1 : static_cast<value_type01>(value2);
	}

	JSONIFIER_INLINE constexpr uint64_t strLen(const char* input) noexcept {
		uint64_t returnVal{};
		if (input) {
			while (input[returnVal] != '\0') {
				++returnVal;
			}
		}
		return returnVal;
	}

	template<typename value_type> struct digit_sizes;

	template<uint64_types value_type> struct digit_sizes<value_type> {
		static constexpr uint64_t value{ 20 };
	};

	template<int64_types value_type> struct digit_sizes<value_type> {
		static constexpr uint64_t value{ 20 };
	};

	template<uint32_types value_type> struct digit_sizes<value_type> {
		static constexpr uint64_t value{ 10 };
	};

	template<int32_types value_type> struct digit_sizes<value_type> {
		static constexpr uint64_t value{ 11 };
	};

	template<uint16_types value_type> struct digit_sizes<value_type> {
		static constexpr uint64_t value{ 5 };
	};

	template<int16_types value_type> struct digit_sizes<value_type> {
		static constexpr uint64_t value{ 6 };
	};

	template<uint8_types value_type> struct digit_sizes<value_type> {
		static constexpr uint64_t value{ 3 };
	};

	template<int8_types value_type> struct digit_sizes<value_type> {
		static constexpr uint64_t value{ 4 };
	};

	template<float32_types value_type> struct digit_sizes<value_type> {
		static constexpr uint64_t value{ 17 };
	};

	template<float64_types value_type> struct digit_sizes<value_type> {
		static constexpr uint64_t value{ 34 };
	};

	template<typename value_type> inline constexpr uint64_t digit_size_v = digit_sizes<value_type>::value;

	template<char_types value_type> struct char_sizes {
		static constexpr uint64_t value{ 16 };
	};

	template<typename value_type> inline constexpr uint64_t char_size_v = char_sizes<value_type>::value;

#if JSONIFIER_PLATFORM_MAC

	template<uint64_t chunkBytes> JSONIFIER_INLINE void copyOverlappingChunks(std::byte* __restrict dst, const std::byte* __restrict src, uint64_t byteCount) {
		pow2MemcpyWrapper<chunkBytes>(dst, src);
		pow2MemcpyWrapper<chunkBytes>(dst + byteCount - chunkBytes, src + byteCount - chunkBytes);
	}

	template<uint64_t maxBytes> JSONIFIER_INLINE void jsonifierMemcpyUpTo(void* __restrict dst, const void* __restrict src, uint64_t byteCount) {
		switch (std::bit_width(byteCount)) {
			case 0: {
				return;
			}
			case 1: {
				*static_cast<std::byte* __restrict>(dst) = *static_cast<const std::byte* __restrict>(src);
				return;
			}
			case 2: {
				if constexpr (maxBytes >= 2) {
					copyOverlappingChunks<2>(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byteCount);
				}
				return;
			}
			case 3: {
				if constexpr (maxBytes >= 4) {
					copyOverlappingChunks<4>(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byteCount);
				}
				return;
			}
			case 4: {
				if constexpr (maxBytes >= 8) {
					copyOverlappingChunks<8>(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byteCount);
				}
				return;
			}
			case 5: {
				if constexpr (maxBytes >= 16) {
					copyOverlappingChunks<16>(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byteCount);
				}
				return;
			}
			case 6: {
				if constexpr (maxBytes >= 32) {
					copyOverlappingChunks<32>(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byteCount);
				}
				return;
			}
			default: {
				if constexpr (maxBytes >= 64) {
					memcpyWrapper(static_cast<std::byte* __restrict>(dst), static_cast<const std::byte* __restrict>(src), byteCount);
				}
				return;
			}
		}
	}

	JSONIFIER_INLINE void jsonifierMemcpy(void* __restrict destination, const void* __restrict source, uint64_t byteCount) {
		jsonifierMemcpyUpTo<std::numeric_limits<uint64_t>::max()>(destination, source, byteCount);
	}

#else

	template<uint64_t bucketSize, typename sequence_type> struct memcpy_runner_impl;

	inline constexpr uint64_t registerWidth{ cpu_properties::getValue(cpu_property_types::alignment) };

	template<uint64_t bucketSize, uint64_t... indices> struct memcpy_runner_impl<bucketSize, integer_sequence<indices...>> {
		static_assert(std::has_single_bit(bucketSize), "Sorry, but buckets must be a power-of-2 size.");
		static constexpr uint64_t chunkSize{ bucketSize < registerWidth ? bucketSize : registerWidth };
		static constexpr uint64_t chunkCount{ bucketSize / chunkSize };

		JSONIFIER_INLINE static void impl(std::byte* __restrict dst, const std::byte* __restrict src, uint64_t byteCount) noexcept {
			std::byte* __restrict dstTail		= dst + byteCount - bucketSize;
			const std::byte* __restrict srcTail = src + byteCount - bucketSize;
			(pow2MemcpyWrapper<chunkSize>(dst + indices * chunkSize, src + indices * chunkSize), ...);
			(pow2MemcpyWrapper<chunkSize>(dstTail + indices * chunkSize, srcTail + indices * chunkSize), ...);
		}
	};

	template<uint64_t bucketSize> struct memcpy_runner {
		static_assert(std::has_single_bit(bucketSize), "Sorry, but buckets must be a power-of-2 size.");
		static constexpr uint64_t chunkSize{ bucketSize < registerWidth ? bucketSize : registerWidth };
		static constexpr uint64_t chunkCount{ bucketSize / chunkSize };

		JSONIFIER_INLINE static void impl(std::byte* __restrict dst, const std::byte* __restrict src, uint64_t byteCount) noexcept {
			memcpy_runner_impl<bucketSize, make_integer_sequence<chunkCount>>::impl(dst, src, byteCount);
		}
	};

	inline constexpr uint64_t fallbackBucketIndex{ 9 };

	template<uint64_t maxBytes> inline constexpr uint64_t maxBytesBucketIndex{ static_cast<uint64_t>(std::bit_width(maxBytes)) };

	template<uint64_t maxBytes>
	inline constexpr uint64_t topBucketIndex{ maxBytesBucketIndex<maxBytes> < fallbackBucketIndex ? maxBytesBucketIndex<maxBytes> : fallbackBucketIndex };

	template<uint64_t bucketIndex> JSONIFIER_INLINE void copyBucket(std::byte* __restrict dst, const std::byte* __restrict src, uint64_t byteCount) noexcept {
		if constexpr (bucketIndex == 1) {
			*dst = *src;
		} else if constexpr (bucketIndex == fallbackBucketIndex) {
			memcpyWrapper(dst, src, byteCount);
		} else if constexpr (bucketIndex > 1) {
			memcpy_runner<1ull << (bucketIndex - 1)>::impl(dst, src, byteCount);
		}
	}

	template<uint64_t lowIndex, uint64_t highIndex> JSONIFIER_INLINE void dispatchBuckets(std::byte* __restrict dst, const std::byte* __restrict src, uint64_t byteCount) noexcept {
		if constexpr (lowIndex == highIndex) {
			copyBucket<lowIndex>(dst, src, byteCount);
		} else {
			static constexpr uint64_t splitIndex{ (lowIndex + highIndex + 1) / 2 };
			if (byteCount < (1ull << (splitIndex - 1))) {
				dispatchBuckets<lowIndex, splitIndex - 1>(dst, src, byteCount);
			} else {
				dispatchBuckets<splitIndex, highIndex>(dst, src, byteCount);
			}
		}
	}

	template<uint64_t maxBytes> JSONIFIER_INLINE void jsonifierMemcpyUpTo(void* __restrict dst, const void* __restrict src, uint64_t byteCount) noexcept {
		dispatchBuckets<0, topBucketIndex<maxBytes>>(static_cast<std::byte*>(dst), static_cast<const std::byte*>(src), byteCount);
	}

	JSONIFIER_INLINE void jsonifierMemcpy(void* __restrict dst, const void* __restrict src, uint64_t byteCount) noexcept {
		jsonifierMemcpyUpTo<std::numeric_limits<uint64_t>::max()>(dst, src, byteCount);
	}

#endif

}

#include <jsonifier-incl/containers/tuple.hpp>

namespace jsonifier::internal::simd {

#if JSONIFIER_NEON || JSONIFIER_SVE2
	using avx_integer_list = type_list_t<type_holder<64, simd_type_wrapper<avx_types::m512>, uint64_t, 64>, type_holder<32, simd_type_wrapper<avx_types::m256>, uint32_t, 32>,
		type_holder<16, simd_type_wrapper<avx_types::m128>, uint64_t, 16>>;
	using avx_list		   = type_list_t<type_holder<64, simd_type_wrapper<avx_types::m512>, uint64_t, std::numeric_limits<uint64_t>::max()>,
		type_holder<32, simd_type_wrapper<avx_types::m256>, uint32_t, std::numeric_limits<uint32_t>::max()>,
		type_holder<16, simd_type_wrapper<avx_types::m128>, uint64_t, std::numeric_limits<uint64_t>::max()>>;
#else
	using avx_integer_list = type_list_t<type_holder<64, simd_type_wrapper<avx_types::m512>, uint64_t, 64>, type_holder<32, simd_type_wrapper<avx_types::m256>, uint32_t, 32>,
		type_holder<16, simd_type_wrapper<avx_types::m128>, uint16_t, 16>>;
	using avx_list		   = type_list_t<type_holder<64, simd_type_wrapper<avx_types::m512>, uint64_t, std::numeric_limits<uint64_t>::max()>,
		type_holder<32, simd_type_wrapper<avx_types::m256>, uint32_t, std::numeric_limits<uint32_t>::max()>,
		type_holder<16, simd_type_wrapper<avx_types::m128>, uint16_t, std::numeric_limits<uint16_t>::max()>>;
#endif

}
