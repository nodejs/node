/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/simd/simd_x.hpp
 */
#pragma once

#include <jsonifier-incl/core/config.hpp>

namespace jsonifier::internal::simd {

	struct simd_x {
	  public:
		static constexpr uint64_t sixtyFourPer{ 16ULL / sizeof(uint64_t) };
		static constexpr uint64_t thirtyTwoPer{ 16ULL / sizeof(uint32_t) };
		static constexpr uint64_t sixteenPer{ 16ULL / sizeof(uint16_t) };
		static constexpr uint64_t eightPer{ 16ULL / sizeof(uint8_t) };

		union alignas(16) storage_type {
			uint64_t xUint64[sixtyFourPer];
			uint32_t xUint32[thirtyTwoPer];
			uint16_t xUint16[sixteenPer];
			uint8_t xUint8[eightPer];
		};

		JSONIFIER_INLINE simd_x() noexcept						   = default;
		JSONIFIER_INLINE simd_x& operator=(const simd_x&) noexcept = default;
		JSONIFIER_INLINE simd_x(const simd_x&) noexcept			   = default;

		storage_type values;
	};

}
