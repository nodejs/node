/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/utilities/byteswap.hpp
 */
#pragma once

#include <jsonifier-incl/core/defines.hpp>
#include <concepts>
#include <type_traits>

namespace jsonifier::internal {

	template<std::integral value_type> JSONIFIER_INLINE constexpr value_type byteswap(value_type value) noexcept {
		if constexpr (sizeof(value_type) == 1) {
			return value;
		} else {
			using unsigned_type = std::make_unsigned_t<value_type>;
			const unsigned_type bits{ static_cast<unsigned_type>(value) };
#if JSONIFIER_COMPILER_GCC || JSONIFIER_COMPILER_CLANG
			if constexpr (sizeof(value_type) == 2) {
				return static_cast<value_type>(__builtin_bswap16(bits));
			} else if constexpr (sizeof(value_type) == 4) {
				return static_cast<value_type>(__builtin_bswap32(bits));
			} else {
				return static_cast<value_type>(__builtin_bswap64(bits));
			}
#else
			if (!std::is_constant_evaluated()) {
				if constexpr (sizeof(value_type) == 2) {
					return static_cast<value_type>(_byteswap_ushort(bits));
				} else if constexpr (sizeof(value_type) == 4) {
					return static_cast<value_type>(_byteswap_ulong(bits));
				} else {
					return static_cast<value_type>(_byteswap_uint64(bits));
				}
			}
			unsigned_type result{};
			for (uint64_t index = 0; index < sizeof(value_type); ++index) {
				result = static_cast<unsigned_type>((result << 8) | ((bits >> (index * 8)) & 0xFF));
			}
			return static_cast<value_type>(result);
#endif
		}
	}

}
