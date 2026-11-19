/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/utilities/compare.hpp
 */
#pragma once

#include <jsonifier-incl/utilities/string_literal.hpp>
#include <jsonifier-incl/utilities/utility.hpp>
#include <jsonifier-incl/utilities/byteswap.hpp>
#include <jsonifier-incl/simd/avx.hpp>
#include <jsonifier-incl/simd/neon.hpp>
#include <jsonifier-incl/simd/sve2.hpp>
#include <jsonifier-incl/simd/fallback.hpp>

namespace jsonifier::internal {

	template<uint8_t repeat, uint_types return_type> consteval return_type repeatByte() noexcept {
		return static_cast<return_type>(0x0101010101010101ull) * static_cast<return_type>(repeat);
	}

	template<uint_types return_type> inline constexpr return_type repeatByte(uint8_t repeat) noexcept {
		return static_cast<return_type>(static_cast<return_type>(0x0101010101010101ull) * static_cast<return_type>(repeat));
	}

	template<char valueNewer, typename char_type> struct char_comparison {
		static constexpr char value{ valueNewer };

		JSONIFIER_INLINE static const char_type* memchar(const char_type* data, uint64_t lengthNew) noexcept {
#if JSONIFIER_AVX512
			if (lengthNew >= 64) {
				using type_list_element				 = type_list_element_t<0, simd::avx_list>;
				using simd_type						 = typename type_list_element::type::type;
				using integer_type					 = typename type_list_element::integer_type;
				static constexpr uint64_t vectorSize = type_list_element::bytesProcessed;
				const simd_type searchValue			 = simd::gatherValue<simd_type>(static_cast<uint8_t>(value));
				integer_type mask;
				simd_type chunk;
				while (lengthNew >= vectorSize) {
					chunk = simd::gatherValuesU<simd_type>(data);
					mask  = simd::opCmpEq(chunk, searchValue);
					if (mask != 0) [[unlikely]] {
						data += simd::postCmpTzcntUnsafe(mask);
						return data;
					}
					lengthNew -= vectorSize;
					data += vectorSize;
				}
			}
#endif

#if JSONIFIER_AVX512 || JSONIFIER_AVX2
			if (lengthNew >= 32) {
				using type_list_element				 = type_list_element_t<1, simd::avx_list>;
				using simd_type						 = typename type_list_element::type::type;
				using integer_type					 = typename type_list_element::integer_type;
				static constexpr uint64_t vectorSize = type_list_element::bytesProcessed;
				const simd_type searchValue			 = simd::gatherValue<simd_type>(static_cast<uint8_t>(value));
				integer_type mask;
				simd_type chunk;
				while (lengthNew >= vectorSize) {
					chunk = simd::gatherValuesU<simd_type>(data);
					mask  = simd::opCmpEq(chunk, searchValue);
					if (mask != 0) [[unlikely]] {
						data += simd::postCmpTzcntUnsafe(mask);
						return data;
					}
					lengthNew -= vectorSize;
					data += vectorSize;
				}
			}
#endif
			if (lengthNew >= 16) {
				using type_list_element				 = type_list_element_t<2, simd::avx_list>;
				using simd_type						 = typename type_list_element::type::type;
				using integer_type					 = typename type_list_element::integer_type;
				static constexpr uint64_t vectorSize = type_list_element::bytesProcessed;
				const simd_type searchValue			 = simd::gatherValue<simd_type>(static_cast<uint8_t>(value));
				integer_type mask;
				simd_type chunk;
				while (lengthNew >= vectorSize) {
					chunk = simd::gatherValuesU<simd_type>(data);
					mask  = static_cast<integer_type>(simd::opCmpEq(chunk, searchValue));
					if (mask != 0) [[unlikely]] {
						data += simd::postCmpTzcntUnsafe(mask);
						return data;
					}
					lengthNew -= vectorSize;
					data += vectorSize;
				}
			}
			{
				static constexpr uint64_t nBytes{ sizeof(uint64_t) };
				if (lengthNew >= nBytes) {
					static constexpr auto valueNew{ repeatByte<value, uint64_t>() };
					static constexpr auto highBits{ repeatByte<0x80, uint64_t>() };
					static constexpr auto lowBits{ repeatByte<0x01, uint64_t>() };
					uint64_t simdValue;
					pow2MemcpyWrapper<sizeof(uint64_t)>(&simdValue, data);
					if constexpr (std::endian::native == std::endian::big) {
						simdValue = internal::byteswap(simdValue);
					}
					const auto chunk = simdValue ^ valueNew;
					auto next		 = ((chunk - lowBits) & ~chunk) & highBits;
					if (next) {
						next = simd::countrZeroUnsafe(static_cast<uint64_t>(next)) >> 3u;
						data += next;
						return data;
					} else {
						lengthNew -= nBytes;
						data += nBytes;
					}
				}
			}
			{
				static constexpr uint64_t nBytes{ sizeof(uint32_t) };
				if (lengthNew >= nBytes) {
					static constexpr auto valueNew{ repeatByte<value, uint32_t>() };
					static constexpr auto highBits{ repeatByte<0x80, uint32_t>() };
					static constexpr auto lowBits{ repeatByte<0x01, uint32_t>() };
					uint32_t simdValue;
					pow2MemcpyWrapper<sizeof(uint32_t)>(&simdValue, data);
					if constexpr (std::endian::native == std::endian::big) {
						simdValue = internal::byteswap(simdValue);
					}
					const auto chunk = simdValue ^ valueNew;
					auto next		 = ((chunk - lowBits) & ~chunk) & highBits;
					if (next) {
						next = static_cast<uint32_t>(simd::countrZeroUnsafe(static_cast<uint32_t>(next)) >> 3u);
						data += next;
						return data;
					} else {
						lengthNew -= nBytes;
						data += nBytes;
					}
				}
			}
			{
				static constexpr uint64_t nBytes{ sizeof(uint16_t) };
				if (lengthNew >= nBytes) {
					static constexpr auto valueNew{ repeatByte<value, uint16_t>() };
					static constexpr auto highBits{ repeatByte<0x80, uint16_t>() };
					static constexpr auto lowBits{ repeatByte<0x01, uint16_t>() };
					uint16_t simdValue;
					pow2MemcpyWrapper<sizeof(uint16_t)>(&simdValue, data);
					if constexpr (std::endian::native == std::endian::big) {
						simdValue = internal::byteswap(simdValue);
					}
					const auto chunk = simdValue ^ valueNew;
					auto next		 = ((chunk - lowBits) & ~chunk) & highBits;
					if (next) {
						next = static_cast<int32_t>(simd::countrZeroUnsafe(static_cast<uint16_t>(next)) >> 3u);
						data += next;
						return data;
					} else {
						lengthNew -= nBytes;
						data += nBytes;
					}
				}
			}

			if (lengthNew > 0 && *data == value) {
				return data;
			}
			return static_cast<decltype(data)>(nullptr);
		}
	};

	struct comparison {
		template<typename char_type01, typename char_type02> JSONIFIER_INLINE static bool compare(const char_type01* lhs, char_type02* rhs, uint64_t lengthNew) noexcept {
#if JSONIFIER_AVX512
			if (lengthNew >= 64) {
				using type_list_element				 = type_list_element_t<0, simd::avx_list>;
				using simd_type						 = typename type_list_element::type::type;
				static constexpr uint64_t vectorSize = type_list_element::bytesProcessed;
				static constexpr uint64_t mask		 = type_list_element::mask;
				simd_type value01, value02;
				while (lengthNew >= vectorSize) {
					value01 = simd::gatherValuesU<simd_type>(lhs);
					value02 = simd::gatherValuesU<simd_type>(rhs);
					if (simd::opCmpEq(value01, value02) != mask) {
						return false;
					}
					lengthNew -= vectorSize;
					lhs += vectorSize;
					rhs += vectorSize;
				}
			}
#endif
#if JSONIFIER_AVX512 || JSONIFIER_AVX2
			if (lengthNew >= 32) {
				using type_list_element				 = type_list_element_t<1, simd::avx_list>;
				using simd_type						 = typename type_list_element::type::type;
				static constexpr uint64_t vectorSize = type_list_element::bytesProcessed;
				static constexpr uint64_t mask		 = type_list_element::mask;
				simd_type value01, value02;
				while (lengthNew >= vectorSize) {
					value01 = simd::gatherValuesU<simd_type>(lhs);
					value02 = simd::gatherValuesU<simd_type>(rhs);
					if (simd::opCmpEq(value01, value02) != mask) {
						return false;
					}
					lengthNew -= vectorSize;
					lhs += vectorSize;
					rhs += vectorSize;
				}
			}
#endif
			if (lengthNew >= 16) {
				using type_list_element				 = type_list_element_t<2, simd::avx_list>;
				using simd_type						 = typename type_list_element::type::type;
				static constexpr uint64_t vectorSize = type_list_element::bytesProcessed;
				static constexpr uint64_t mask		 = type_list_element::mask;
				simd_type value01, value02;
				while (lengthNew >= vectorSize) {
					value01 = simd::gatherValuesU<simd_type>(lhs);
					value02 = simd::gatherValuesU<simd_type>(rhs);
					if (simd::opCmpEq(value01, value02) != mask) {
						return false;
					}
					lengthNew -= vectorSize;
					lhs += vectorSize;
					rhs += vectorSize;
				}
			}
			{
				static constexpr uint64_t nBytes{ sizeof(uint64_t) };
				if (lengthNew >= nBytes) {
					uint64_t v1, v2;
					pow2MemcpyWrapper<nBytes>(&v1, lhs);
					pow2MemcpyWrapper<nBytes>(&v2, rhs);
					if ((v1 ^ v2) != 0) {
						return false;
					}
					lengthNew -= nBytes;
					lhs += nBytes;
					rhs += nBytes;
				}
			}
			{
				static constexpr uint64_t nBytes{ sizeof(uint32_t) };
				if (lengthNew >= nBytes) {
					uint32_t v1, v2;
					pow2MemcpyWrapper<nBytes>(&v1, lhs);
					pow2MemcpyWrapper<nBytes>(&v2, rhs);
					if ((v1 ^ v2) != 0) {
						return false;
					}
					lengthNew -= nBytes;
					lhs += nBytes;
					rhs += nBytes;
				}
			}
			{
				static constexpr uint64_t nBytes{ sizeof(uint16_t) };
				if (lengthNew >= nBytes) {
					uint16_t v1, v2;
					pow2MemcpyWrapper<nBytes>(&v1, lhs);
					pow2MemcpyWrapper<nBytes>(&v2, rhs);
					if ((v1 ^ v2) != 0) {
						return false;
					}
					lengthNew -= nBytes;
					lhs += nBytes;
					rhs += nBytes;
				}
			}
			if (lengthNew && *lhs != *rhs) {
				return false;
			}
			return true;
		}
	};

	template<uint64_t length> struct convert_length_to_int {
		using type = conditional_t<length == 1, uint8_t, conditional_t<length <= 2, uint16_t, conditional_t<length <= 4, uint32_t, conditional_t<length <= 8, uint64_t, void>>>>;
	};

	template<uint64_t length> using convert_length_to_int_t = typename convert_length_to_int<length>::type;

	template<uint64_t length> inline constexpr uint64_t intLength = []<uint64_t lengthNew> {
		if constexpr (lengthNew == 1ULL) {
			return 1ULL;
		} else if constexpr (lengthNew == 2ULL) {
			return 2ULL;
		} else if constexpr (lengthNew <= 4ULL && lengthNew > 2ULL) {
			return 4ULL;
		} else if constexpr (lengthNew <= 8ULL && lengthNew > 4ULL) {
			return 8ULL;
		} else if constexpr (lengthNew <= 16ULL && lengthNew > 8ULL) {
			return 16ULL;
		} else {
			return lengthNew;
		}
	}.template operator()<length>();

	template<auto string, bool = (base_t<decltype(string)>::length > 8)> struct pack_values;

	template<auto string> struct pack_values<string, false> {
		static consteval decltype(auto) gen() noexcept {
			using int_type = convert_length_to_int_t<string.length>;
			int_type returnValues{};
			for (uint64_t x = 0; x < string.length; ++x) {
				if constexpr (std::endian::native == std::endian::little) {
					returnValues |= static_cast<int_type>(static_cast<uint64_t>(static_cast<uint8_t>(string[x])) << ((x % 8) * 8));
				} else {
					returnValues |= static_cast<int_type>(static_cast<uint64_t>(static_cast<uint8_t>(string[x])) << (((sizeof(int_type) - 1 - (x % 8))) * 8));
				}
			}
			return returnValues;
		}

		static constexpr auto value{ gen() };
	};

	template<auto string> struct pack_values<string, true> {
		using array_type = array<uint8_t, intLength<string.size()>>;

		static consteval array_type gen() noexcept {
			array_type outLocal{};
			for (uint64_t i = 0; i < string.size(); ++i) {
				outLocal[i] = static_cast<uint8_t>(string[i]);
			}
			return outLocal;
		}

		alignas(simdBytesPerRegister) static constexpr auto value{ gen() };
	};

	struct string_status {
		uint64_t remainingLength{};
		uint64_t currentIndex{};
	};

	template<auto string, uint64_t offset> consteval string_literal<((offset >= string.length) ? string.length : offset) + 1> offSetIntoLiteral() {
		constexpr uint64_t newSize = ((offset >= string.length) ? string.length : offset);
		string_literal<newSize + 1> sl{};
		if constexpr (newSize > 0) {
			std::copy_n(string.data(), newSize, sl.values);
			sl.values[newSize] = '\0';
		}
		return sl;
	}

	template<auto string, uint64_t offset> consteval string_literal<((offset >= string.length) ? 0 : string.length - offset) + 1> offSetNewLiteral() {
		constexpr uint64_t newSize = (offset >= string.length) ? 0 : string.length - offset;
		string_literal<newSize + 1> sl{};
		if constexpr (newSize > 0) {
			std::copy_n(string.data() + offset, newSize, sl.values);
			sl.values[newSize] = '\0';
		}
		return sl;
	}

	template<uint64_t splitMod, uint64_t size> consteval uint64_t getSplitStringCount(const string_literal<size>&) noexcept {
		constexpr uint64_t splitCount = (size + splitMod - 1ull) / splitMod;
		return splitCount == 0ull ? 1ull : splitCount;
	}

	template<string_status status, auto stringNew, uint64_t splitMod> struct string_literal_splitter_impl {
		static constexpr uint64_t currentLength{ splitMod < status.remainingLength ? splitMod : status.remainingLength };
		static constexpr auto string{ offSetIntoLiteral<offSetNewLiteral<stringNew, status.currentIndex>(), currentLength>() };
	};

	template<typename sequence, auto stringNew, uint64_t splitMod> struct string_literal_splitter;

	consteval auto getOffsetIntoLiteralSize(uint64_t inputSize) noexcept {
		if (inputSize >= 64ULL && simdBytesPerRegister >= 64ULL) {
			return 64ULL;
		} else if (inputSize >= 32ULL && simdBytesPerRegister >= 32ULL) {
			return 32ULL;
		} else {
			return 16ULL;
		}
	}

	template<typename sl_type, base_t<sl_type> stringNew, typename = void> struct string_literal_comparator_impl;

	template<uint64_t... indices, auto stringNew, uint64_t splitMod> struct string_literal_splitter<integer_sequence<indices...>, stringNew, splitMod> {
		JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> impl(read_buffer_ptr str) noexcept {
			return (static_cast<void>(
						((str = string_literal_comparator_impl<decltype(string_literal_splitter_impl<getSplitStringStatus(indices, stringNew.size()), stringNew, splitMod>::string),
							  string_literal_splitter_impl<getSplitStringStatus(indices, stringNew.size()), stringNew, splitMod>::string>::impl(str),
							 str != nullptr) &&
							...)),
				str);
		}

		static consteval string_status getSplitStringStatus(uint64_t index, uint64_t totalLength) noexcept {
			uint64_t currentIndex{ splitMod * index };
			string_status status{ totalLength, currentIndex };
			status.remainingLength -= currentIndex;
			return status;
		}
	};

	template<eq_0_types sl_type, base_t<sl_type> stringNew> struct string_literal_comparator_impl<sl_type, stringNew, void> {
		JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> impl(read_buffer_ptr str) noexcept {
			return str;
		}
	};

	template<eq_1_types sl_type, base_t<sl_type> stringNew> struct string_literal_comparator_impl<sl_type, stringNew, void> {
		JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> impl(read_buffer_ptr str) noexcept {
			static constexpr auto stringLiteral{ stringNew };
			static constexpr auto newCount{ stringLiteral.size() };
			return (*str == stringLiteral[0]) ? str + newCount : nullptr;
		}
	};

	template<eq_2_types sl_type, base_t<sl_type> stringNew> struct string_literal_comparator_impl<sl_type, stringNew, void> {
		JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> impl(read_buffer_ptr str) noexcept {
			static constexpr auto stringLiteral{ stringNew };
			static constexpr auto newCount{ stringLiteral.size() };
			static constexpr uint16_t valuesNew{ pack_values<stringLiteral>::value };
			uint16_t l;
			pow2MemcpyWrapper<2>(&l, str);
			return !(l ^ valuesNew) ? str + newCount : nullptr;
		}
	};

	template<eq_3_types sl_type, base_t<sl_type> stringNew> struct string_literal_comparator_impl<sl_type, stringNew, void> {
		JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> impl(read_buffer_ptr str) noexcept {
			static constexpr auto stringLiteral{ stringNew };
			static constexpr auto newCount{ stringLiteral.size() };
			static constexpr auto loString = stringLiteral.template substr<0, 2>();
			static constexpr auto hiString = stringLiteral.template substr<1, 2>();
			static constexpr uint16_t loValuesNew{ pack_values<loString>::value };
			static constexpr uint16_t hiValuesNew{ pack_values<hiString>::value };
			uint16_t lo, hi;
			pow2MemcpyWrapper<2>(&lo, str);
			pow2MemcpyWrapper<2>(&hi, str + 1);
			return !((lo ^ loValuesNew) | (hi ^ hiValuesNew)) ? str + newCount : nullptr;
		}
	};

	template<eq_4_types sl_type, base_t<sl_type> stringNew> struct string_literal_comparator_impl<sl_type, stringNew, void> {
		JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> impl(read_buffer_ptr str) noexcept {
			static constexpr auto stringLiteral{ stringNew };
			static constexpr auto newCount{ stringLiteral.size() };
			static constexpr uint32_t valuesNew{ pack_values<stringLiteral>::value };
			uint32_t l;
			pow2MemcpyWrapper<4>(&l, str);
			return !(l ^ valuesNew) ? str + newCount : nullptr;
		}
	};

	template<eq_5_types sl_type, base_t<sl_type> stringNew> struct string_literal_comparator_impl<sl_type, stringNew, void> {
		JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> impl(read_buffer_ptr str) noexcept {
			static constexpr auto stringLiteral{ stringNew };
			static constexpr auto newCount{ stringLiteral.size() };
			static constexpr auto loString = stringLiteral.template substr<0, 4>();
			static constexpr auto hiString = stringLiteral.template substr<1, 4>();
			static constexpr uint32_t loValuesNew{ pack_values<loString>::value };
			static constexpr uint32_t hiValuesNew{ pack_values<hiString>::value };
			uint32_t lo, hi;
			pow2MemcpyWrapper<4>(&lo, str);
			pow2MemcpyWrapper<4>(&hi, str + 1);
			return !((lo ^ loValuesNew) | (hi ^ hiValuesNew)) ? str + newCount : nullptr;
		}
	};

	template<eq_6_types sl_type, base_t<sl_type> stringNew> struct string_literal_comparator_impl<sl_type, stringNew, void> {
		JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> impl(read_buffer_ptr str) noexcept {
			static constexpr auto stringLiteral{ stringNew };
			static constexpr auto newCount{ stringLiteral.size() };
			static constexpr auto loString = stringLiteral.template substr<0, 4>();
			static constexpr auto hiString = stringLiteral.template substr<2, 4>();
			static constexpr uint32_t loValuesNew{ pack_values<loString>::value };
			static constexpr uint32_t hiValuesNew{ pack_values<hiString>::value };
			uint32_t lo, hi;
			pow2MemcpyWrapper<4>(&lo, str);
			pow2MemcpyWrapper<4>(&hi, str + 2);
			return !((lo ^ loValuesNew) | (hi ^ hiValuesNew)) ? str + newCount : nullptr;
		}
	};

	template<eq_7_types sl_type, base_t<sl_type> stringNew> struct string_literal_comparator_impl<sl_type, stringNew, void> {
		JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> impl(read_buffer_ptr str) noexcept {
			static constexpr auto stringLiteral{ stringNew };
			static constexpr auto newCount{ stringLiteral.size() };
			static constexpr auto loString = stringLiteral.template substr<0, 4>();
			static constexpr auto hiString = stringLiteral.template substr<3, 4>();
			static constexpr uint32_t loValuesNew{ pack_values<loString>::value };
			static constexpr uint32_t hiValuesNew{ pack_values<hiString>::value };
			uint32_t lo, hi;
			pow2MemcpyWrapper<4>(&lo, str);
			pow2MemcpyWrapper<4>(&hi, str + 3);
			return !((lo ^ loValuesNew) | (hi ^ hiValuesNew)) ? str + newCount : nullptr;
		}
	};

	template<eq_8_types sl_type, base_t<sl_type> stringNew> struct string_literal_comparator_impl<sl_type, stringNew, void> {
		JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> impl(read_buffer_ptr str) noexcept {
			static constexpr auto stringLiteral{ stringNew };
			static constexpr auto newCount{ stringLiteral.size() };
			static constexpr uint64_t valuesNew{ pack_values<stringLiteral>::value };
			uint64_t l;
			pow2MemcpyWrapper<8>(&l, str);
			return !(l ^ valuesNew) ? str + newCount : nullptr;
		}
	};

	template<gt_8_lt_16_types sl_type, base_t<sl_type> stringNew> struct string_literal_comparator_impl<sl_type, stringNew, void> {
		JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> impl(read_buffer_ptr str) noexcept {
			static constexpr auto stringLiteral{ stringNew };
			static constexpr auto newCount{ stringLiteral.size() };
			static constexpr auto loString = stringLiteral.template substr<0, 8>();
			static constexpr uint64_t loValuesNew{ pack_values<loString>::value };
#if JSONIFIER_ARCH_X64
			static constexpr uint64_t tailBytes{ std::bit_ceil(newCount - 8) };
			using tail_type					 = convert_length_to_int_t<tailBytes>;
			static constexpr auto tailString = stringLiteral.template substr<newCount - tailBytes, tailBytes>();
			static constexpr tail_type tailValuesNew{ pack_values<tailString>::value };
			uint64_t lo;
			tail_type tail;
			pow2MemcpyWrapper<8>(&lo, str);
			pow2MemcpyWrapper<tailBytes>(&tail, str + newCount - tailBytes);
			return !((lo ^ loValuesNew) | static_cast<uint64_t>(static_cast<tail_type>(tail ^ tailValuesNew))) ? str + newCount : nullptr;
#else
			static constexpr auto hiString = stringLiteral.template substr<newCount - 8, 8>();
			static constexpr uint64_t hiValuesNew{ pack_values<hiString>::value };
			uint64_t lo, hi;
			pow2MemcpyWrapper<8>(&lo, str);
			pow2MemcpyWrapper<8>(&hi, str + newCount - 8);
			return !((lo ^ loValuesNew) | (hi ^ hiValuesNew)) ? str + newCount : nullptr;
#endif
		}
	};

	template<eq_16_types sl_type, base_t<sl_type> stringNew> struct string_literal_comparator_impl<sl_type, stringNew, void> {
		JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> impl(read_buffer_ptr str) noexcept {
			static constexpr auto newLiteral{ stringNew };
			alignas(simdBytesPerRegister) static constexpr auto valuesNew{ pack_values<newLiteral>::value };
			const jsonifier_simd_int_128 data1{ simd::gatherValuesU<jsonifier_simd_int_128>(str) };
			const jsonifier_simd_int_128 data2{ simd::gatherValues<jsonifier_simd_int_128>(valuesNew.data()) };
			return simd::opTest(simd::opXor(data1, data2)) ? str + 16 : nullptr;
		}
	};

	template<gt_16_lt_32_types sl_type, base_t<sl_type> stringNew> struct string_literal_comparator_impl<sl_type, stringNew, void> {
		JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> impl(read_buffer_ptr str) noexcept {
			static constexpr auto stringLiteral{ stringNew };
			static constexpr auto newCount{ stringLiteral.size() };
			static constexpr auto loString = stringLiteral.template substr<0, 16>();
			static constexpr auto hiString = stringLiteral.template substr<newCount - 16, 16>();
			alignas(simdBytesPerRegister) static constexpr auto loValuesNew{ pack_values<loString>::value };
			alignas(simdBytesPerRegister) static constexpr auto hiValuesNew{ pack_values<hiString>::value };
			const jsonifier_simd_int_128 lo{ simd::opXor(simd::gatherValuesU<jsonifier_simd_int_128>(str), simd::gatherValues<jsonifier_simd_int_128>(loValuesNew.data())) };
			const jsonifier_simd_int_128 hi{ simd::opXor(simd::gatherValuesU<jsonifier_simd_int_128>(str + newCount - 16),
				simd::gatherValues<jsonifier_simd_int_128>(hiValuesNew.data())) };
			return simd::opTest(simd::opOr(lo, hi)) ? str + newCount : nullptr;
		}
	};

#if JSONIFIER_AVX512 || JSONIFIER_AVX2

	template<eq_32_types sl_type, base_t<sl_type> stringNew> struct string_literal_comparator_impl<sl_type, stringNew, void> {
		JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> impl(read_buffer_ptr str) noexcept {
			static constexpr auto newLiteral{ stringNew };
			alignas(simdBytesPerRegister) static constexpr auto valuesNew{ pack_values<newLiteral>::value };
			const jsonifier_simd_int_256 data1{ simd::gatherValuesU<jsonifier_simd_int_256>(str) };
			const jsonifier_simd_int_256 data2{ simd::gatherValues<jsonifier_simd_int_256>(valuesNew.data()) };
			return simd::opTest(simd::opXor(data1, data2)) ? str + 32 : nullptr;
		}
	};

	template<gt_32_lt_64_types sl_type, base_t<sl_type> stringNew> struct string_literal_comparator_impl<sl_type, stringNew, void> {
		JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> impl(read_buffer_ptr str) noexcept {
			static constexpr auto stringLiteral{ stringNew };
			static constexpr auto newCount{ stringLiteral.size() };
			static constexpr auto loString = stringLiteral.template substr<0, 32>();
			static constexpr auto hiString = stringLiteral.template substr<newCount - 32, 32>();
			alignas(simdBytesPerRegister) static constexpr auto loValuesNew{ pack_values<loString>::value };
			alignas(simdBytesPerRegister) static constexpr auto hiValuesNew{ pack_values<hiString>::value };
			const jsonifier_simd_int_256 lo{ simd::opXor(simd::gatherValuesU<jsonifier_simd_int_256>(str), simd::gatherValues<jsonifier_simd_int_256>(loValuesNew.data())) };
			const jsonifier_simd_int_256 hi{ simd::opXor(simd::gatherValuesU<jsonifier_simd_int_256>(str + newCount - 32),
				simd::gatherValues<jsonifier_simd_int_256>(hiValuesNew.data())) };
			return simd::opTest(simd::opOr(lo, hi)) ? str + newCount : nullptr;
		}
	};

#endif

#if JSONIFIER_AVX512
	template<eq_64_types sl_type, base_t<sl_type> stringNew> struct string_literal_comparator_impl<sl_type, stringNew, void> {
		JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> impl(read_buffer_ptr str) noexcept {
			static constexpr auto newLiteral{ stringNew };
			alignas(simdBytesPerRegister) static constexpr auto valuesNew{ pack_values<newLiteral>::value };
			const jsonifier_simd_int_512 data1{ simd::gatherValuesU<jsonifier_simd_int_512>(str) };
			const jsonifier_simd_int_512 data2{ simd::gatherValues<jsonifier_simd_int_512>(valuesNew.data()) };
			return simd::opCmpNeq(data1, data2) == 0 ? str + 64 : nullptr;
		}
	};
#endif

	template<gt_16_types sl_type, base_t<sl_type> stringNew> struct string_literal_comparator_impl<sl_type, stringNew, void> {
		static constexpr uint64_t splitMod{ getOffsetIntoLiteralSize(stringNew.size()) };
		static constexpr auto stringCount{ getSplitStringCount<splitMod>(stringNew) };

		JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> impl(read_buffer_ptr str) noexcept {
			return string_literal_splitter<make_integer_sequence<stringCount>, stringNew, splitMod>::impl(str);
		}
	};

	template<auto string> struct string_literal_comparator {
		JSONIFIER_INLINE static bool impl(read_buffer_ptr str, uint64_t stringLength) noexcept {
			using sl_type = decltype(string);
			return stringLength >= string.size() ? string_literal_comparator_impl<sl_type, string>::impl(str) != nullptr : false;
		}
	};

}
