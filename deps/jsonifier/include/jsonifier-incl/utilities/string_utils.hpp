/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/utilities/string_utils.hpp
 */
#pragma once

#include <jsonifier-incl/simd/utf8_validation.hpp>
#include <jsonifier-incl/containers/allocator.hpp>
#include <jsonifier-incl/utilities/str_to_d.hpp>
#include <jsonifier-incl/utilities/error.hpp>
#include <jsonifier-incl/utilities/simd.hpp>
#include <jsonifier-incl/utilities/byteswap.hpp>

namespace jsonifier::internal {

	template<typename = void> alignas(64) inline constexpr uint32_t digitToVal32[]{ 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0x0u, 0x1u, 0x2u, 0x3u, 0x4u, 0x5u, 0x6u, 0x7u, 0x8u, 0x9u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xAu, 0xBu, 0xCu, 0xDu, 0xEu, 0xFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xAu, 0xBu, 0xCu, 0xDu, 0xEu, 0xFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x0u, 0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u, 0x70u, 0x80u, 0x90u, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xA0u, 0xB0u, 0xC0u, 0xD0u, 0xE0u, 0xF0u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xA0u, 0xB0u, 0xC0u, 0xD0u, 0xE0u, 0xF0u, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x0u, 0x100u, 0x200u, 0x300u, 0x400u,
		0x500u, 0x600u, 0x700u, 0x800u, 0x900u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xA00u, 0xB00u, 0xC00u, 0xD00u, 0xE00u,
		0xF00u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xA00u, 0xB00u, 0xC00u, 0xD00u, 0xE00u, 0xF00u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x0u, 0x1000u, 0x2000u, 0x3000u, 0x4000u, 0x5000u, 0x6000u, 0x7000u, 0x8000u, 0x9000u, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xA000u, 0xB000u, 0xC000u, 0xD000u, 0xE000u, 0xF000u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xA000u, 0xB000u, 0xC000u, 0xD000u, 0xE000u, 0xF000u,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
		0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };

	// Sampled from Stephen Berry's library, Glaze: https://github.com/StephenBerry/Glaze
	template<typename basic_iterator01> [[maybe_unused]] JSONIFIER_INLINE void skipStringImpl(basic_iterator01& string1, uint64_t lengthNew) noexcept {
		if (static_cast<int64_t>(lengthNew) > 0) {
			const auto endIter = string1 + lengthNew;
			while (string1 < endIter) {
				auto* newIter = char_comparison<'"', base_t<decltype(*string1)>>::memchar(string1, lengthNew);
				if (newIter) {
					string1	  = newIter;
					lengthNew = static_cast<uint64_t>(endIter - string1);

					auto* prev = string1 - 1;
					while (*prev == '\\') {
						--prev;
					}
					if (static_cast<uint64_t>(string1 - prev) % 2) {
						break;
					}
					++string1;
					lengthNew = static_cast<uint64_t>(endIter - string1);
				} else {
					break;
				}
			}
		}
	}

	// Sampled from Dr. Lemire's library, simdjson: https://github.com/simdjson/simdjson
	JSONIFIER_INLINE uint32_t hexToU32NoCheck(read_buffer_ptr string1) noexcept {
		return digitToVal32<>[630ull + static_cast<uint8_t>(string1[0])] | digitToVal32<>[420ull + static_cast<uint8_t>(string1[1])] |
			digitToVal32<>[210ull + static_cast<uint8_t>(string1[2])] | digitToVal32<>[0ull + static_cast<uint8_t>(string1[3])];
	}

	// Sampled from Dr. Lemire's library, simdjson: https://github.com/simdjson/simdjson
	template<typename char_type> JSONIFIER_INLINE uint32_t codePointToUtf8(uint32_t cp, char_type* c) noexcept {
		if (cp <= 0x7F) {
			c[0] = static_cast<char_type>(cp);
			return 1;
		}
		if (cp <= 0x7FF) {
			c[0] = static_cast<char_type>(0xC0 | ((cp >> 6) & 0x1F));
			c[1] = static_cast<char_type>(0x80 | (cp & 0x3F));
			return 2;
		}
		if (cp <= 0xFFFF) {
			c[0] = static_cast<char_type>(0xE0 | ((cp >> 12) & 0x0F));
			c[1] = static_cast<char_type>(0x80 | ((cp >> 6) & 0x3F));
			c[2] = static_cast<char_type>(0x80 | (cp & 0x3F));
			return 3;
		}
		if (cp <= 0x10FFFF) {
			c[0] = static_cast<char_type>(0xF0 | ((cp >> 18) & 0x07));
			c[1] = static_cast<char_type>(0x80 | ((cp >> 12) & 0x3F));
			c[2] = static_cast<char_type>(0x80 | ((cp >> 6) & 0x3F));
			c[3] = static_cast<char_type>(0x80 | (cp & 0x3F));
			return 4;
		}
		return 0;
	}

	// Sampled from Dr. Lemire's library, simdjson: https://github.com/simdjson/simdjson
	template<typename basic_iterator01, typename basic_iterator02>
	JSONIFIER_INLINE bool handleUnicodeCodePoint(basic_iterator01& srcPtr, basic_iterator02& dstPtr, basic_iterator01 srcEnd) noexcept {
		static constexpr uint8_t bs{ uint_c_v<'\\'> };
		static constexpr uint8_t u{ uint_c_v<'u'> };

		if ((srcPtr + 6) > srcEnd) [[unlikely]] {
			return false;
		}

		uint32_t codePoint = hexToU32NoCheck(srcPtr + 2);
		srcPtr += 6;
		if (codePoint >= 0xD800 && codePoint <= 0xDFFF) {
			if (codePoint >= 0xDC00) {
				return false;
			}
			if ((srcPtr + 6) > srcEnd) [[unlikely]] {
				return false;
			}
			if (((srcPtr[0] << 8) | srcPtr[1]) != ((bs << 8) | u)) {
				return false;
			}
			uint32_t lowSurrogate = hexToU32NoCheck(srcPtr + 2);
			uint32_t lowBit		  = lowSurrogate - 0xDC00;
			if (lowBit >> 10) {
				return false;
			}
			codePoint = (((codePoint - 0xD800) << 10) | lowBit) + 0x10000;
			srcPtr += 6;
		}
		const uint64_t offset = codePointToUtf8(codePoint, dstPtr);
		dstPtr += offset;
		return offset > 0;
	}

	consteval array<char, 256ULL> genEscapeMap() noexcept {
		array<char, 256ULL> returnValues{};
		returnValues[uint_c_v<'"'>]	 = '\"';
		returnValues[uint_c_v<'\\'>] = '\\';
		returnValues[uint_c_v<'/'>]	 = '/';
		returnValues[uint_c_v<'b'>]	 = '\b';
		returnValues[uint_c_v<'f'>]	 = '\f';
		returnValues[uint_c_v<'n'>]	 = '\n';
		returnValues[uint_c_v<'r'>]	 = '\r';
		returnValues[uint_c_v<'t'>]	 = '\t';
		return returnValues;
	}

	alignas(64) inline constexpr auto escapeMapRaw{ genEscapeMap() };

	inline constexpr const char* __restrict escapeMap{ escapeMapRaw.data() };

	template<typename basic_iterator01, typename basic_iterator02>
	JSONIFIER_INLINE basic_iterator02 unescapeImpl(basic_iterator01 string1Start, const basic_iterator01 string1End, basic_iterator02 string2) noexcept {
		char escapeChar;
		while (string1Start < string1End) {
			escapeChar = *string1Start;
			if (escapeChar == '\\') {
				escapeChar = string1Start[1];
				if (escapeChar == 'u') {
					if (!handleUnicodeCodePoint(string1Start, string2, string1End)) {
						return nullptr;
					}
					continue;
				}
				escapeChar = escapeMap[static_cast<uint64_t>(escapeChar)];
				if (escapeChar == 0) {
					return nullptr;
				}
				*string2 = escapeChar;
				++string2;
				string1Start += 2;
				continue;
			}
			*string2 = escapeChar;
			++string2;
			++string1Start;
		}
		return string2;
	}

	template<typename executor_type, typename integer_sequence> struct string_parse_executor;

	template<typename executor_type, uint64_t... indices> struct string_parse_executor<executor_type, integer_sequence<indices...>> {
		template<typename... arg_types> JSONIFIER_INLINE static bool impl(arg_types&&... args) noexcept {
			return ((executor_type::template impl<indices>(std::forward<arg_types>(args)...) != nullptr) && ...);
		}
	};

#if JSONIFIER_AVX512
	inline constexpr uint64_t startIndex = 0;
#elif JSONIFIER_AVX2
	inline constexpr uint64_t startIndex = 1;
#else
	inline constexpr uint64_t startIndex = 2;
#endif

	inline constexpr uint64_t listSize = 3;

	template<uint64_t start, uint64_t end, uint64_t... indices> struct make_ascending_range_impl : make_ascending_range_impl<start + 1, end, indices..., start> {};
	template<uint64_t end, uint64_t... indices> struct make_ascending_range_impl<end, end, indices...> {
		using type = integer_sequence<indices...>;
	};

	template<uint64_t start, uint64_t end> using make_ascending_range = typename make_ascending_range_impl<start, end>::type;

	template<parse_options options> struct string_scanner {
		struct scan_result {
			uint64_t outLength{ std::numeric_limits<uint64_t>::max() };
			uint64_t rawLength{};
		};

		static constexpr uint64_t npos{ std::numeric_limits<uint64_t>::max() };

		struct scan_state {
			utf8_validation_state validationState{};
			uint64_t rawLength{};
			uint64_t outLength{};
			bool complete{};
		};

		struct string_scan_step {
			template<uint64_t index, typename basic_iterator01, typename basic_iterator02> JSONIFIER_INLINE static basic_iterator01 impl(basic_iterator01& string1Start,
				const basic_iterator01 string1End, basic_iterator02& string2, const basic_iterator01 stringStart, const basic_iterator02 outStart, scan_state& scanState) noexcept {
				using simd_list_local					 = type_list_element_t<index, simd::avx_integer_list>;
				using integer_type						 = typename simd_list_local::integer_type;
				static constexpr auto avx_type			 = simd_list_local::type::avx_type;
				using simd_type_local					 = typename simd_type_wrapper<avx_type>::type;
				static constexpr uint64_t bytesProcessed = simd_list_local::bytesProcessed;
				if (scanState.complete || scanState.outLength == std::numeric_limits<uint64_t>::max()) {
					return nullptr;
				}
				utf8_register_validator<simd_type_wrapper<avx_type>> validator{ scanState.validationState };
				const simd_type_local simdValues00 = simd::gatherValue<simd_type_local>('\\');
				const simd_type_local simdValues01 = simd::gatherValue<simd_type_local>('"');
				const simd_type_local simdValues02 = simd::gatherValue<simd_type_local>(static_cast<char>(32));
				const auto stringEndNew			   = string1End - bytesProcessed;
				while (string1Start < stringEndNew) {
					const auto registerStart		= string1Start;
					const simd_type_local simdValue = simd::gatherValuesU<simd_type_local>(string1Start);
					simd::storeU(simdValue, string2);
					const integer_type delimiters = static_cast<integer_type>(simd::opBitMask(
						simd::opOr(simd::opOr(simd::opCmpEqRaw(simdValue, simdValues00), simd::opCmpEqRaw(simdValue, simdValues01)), simd::opCmpLtRaw(simdValue, simdValues02))));
					if (delimiters == static_cast<integer_type>(0)) {
						validator.checkRegister(simdValue);
						string1Start += bytesProcessed;
						string2 += bytesProcessed;
						continue;
					}
					const uint64_t offset	= static_cast<uint64_t>(simd::countrZeroUnsafe(delimiters));
					const uint8_t foundChar = static_cast<uint8_t>(string1Start[offset]);
					if (foundChar < 32) [[unlikely]] {
						scanState.outLength = std::numeric_limits<uint64_t>::max();
						scanState.complete	= true;
						return nullptr;
					}
					validator.checkPartial(registerStart, offset);
					string1Start += offset;
					string2 += offset;
					if (validator.errors()) [[unlikely]] {
						scanState.outLength = std::numeric_limits<uint64_t>::max();
						scanState.complete	= true;
						return nullptr;
					}
					if (foundChar == uint_c_v<'"'>) {
						scanState.rawLength = static_cast<uint64_t>(string1Start - stringStart);
						scanState.outLength = static_cast<uint64_t>(string2 - outStart);
						scanState.complete	= true;
						return nullptr;
					}
					if (string1Start + 1 >= string1End) [[unlikely]] {
						scanState.outLength = std::numeric_limits<uint64_t>::max();
						scanState.complete	= true;
						return nullptr;
					}
					if (!handleEscape(string1Start, string1End, string2)) [[unlikely]] {
						scanState.outLength = std::numeric_limits<uint64_t>::max();
						scanState.complete	= true;
						return nullptr;
					}
					validator.reset();
				}
				validator.flush();

				if constexpr (index == 2) {
					auto validateFrom = string1Start;

					while (string1Start < string1End) {
						const uint8_t currentChar = static_cast<uint8_t>(*string1Start);

						if (currentChar == uint_c_v<'"'>) {
							validator.checkPartial(validateFrom, static_cast<uint64_t>(string1Start - validateFrom));
							if (validator.errors()) [[unlikely]] {
								break;
							}
							validator.flush();
							scanState.rawLength = static_cast<uint64_t>(string1Start - stringStart);
							scanState.outLength = static_cast<uint64_t>(string2 - outStart);
							scanState.complete	= true;
							return nullptr;
						}

						if (currentChar == uint_c_v<'\\'>) {
							validator.checkPartial(validateFrom, static_cast<uint64_t>(string1Start - validateFrom));
							if (validator.errors()) [[unlikely]] {
								break;
							}
							if (string1Start + 1 >= string1End) [[unlikely]] {
								break;
							}
							if (!handleEscape(string1Start, string1End, string2)) [[unlikely]] {
								break;
							}
							validator.reset();
							validateFrom = string1Start;
							continue;
						}

						if (currentChar < 32) [[unlikely]] {
							break;
						}

						*string2 = static_cast<remove_cvref_t<decltype(*string2)>>(currentChar);
						++string2;
						++string1Start;
					}

					validator.flush();
					scanState.outLength = std::numeric_limits<uint64_t>::max();
					scanState.complete	= true;
				}

				return string1Start;
			}
		};

		template<typename basic_iterator01, typename basic_iterator02>
		JSONIFIER_INLINE static scan_result impl(basic_iterator01 string1Start, const basic_iterator01 string1End, basic_iterator02 string2) noexcept {
			const auto stringStart = string1Start;
			const auto outStart	   = string2;
			scan_state scanState{};

			string_parse_executor<string_scan_step, make_ascending_range<startIndex, listSize>>::impl(string1Start, string1End, string2, stringStart, outStart, scanState);

			if (scanState.outLength == std::numeric_limits<uint64_t>::max()) [[unlikely]] {
				return {};
			}
			return { scanState.outLength, scanState.rawLength };
		}

		template<typename basic_iterator01, typename basic_iterator02>
		JSONIFIER_INLINE static bool handleEscape(basic_iterator01& srcPtr, const basic_iterator01 srcEnd, basic_iterator02& dstPtr) noexcept {
			const uint8_t escapeChar = static_cast<uint8_t>(srcPtr[1]);
			if (escapeChar == uint_c_v<'u'>) {
				return handleUnicodeCodePoint(srcPtr, dstPtr, srcEnd);
			}
			const uint8_t decoded = static_cast<uint8_t>(escapeMap[escapeChar]);
			if (decoded == 0u) [[unlikely]] {
				return false;
			}
			*dstPtr = static_cast<remove_cvref_t<decltype(*dstPtr)>>(decoded);
			++dstPtr;
			srcPtr += 2;
			return true;
		}
	};

	template<parse_options options> struct string_prescanner {
		struct prescan_result {
			uint64_t length{};
			bool nonAscii{};
			bool escaped{};
			bool control{};
			bool found{};
		};

		struct scan_state {
			prescan_result result{};
			bool complete{};
		};

		struct string_prescan_step {
			template<uint64_t index, typename basic_iterator01> JSONIFIER_INLINE static basic_iterator01 impl(basic_iterator01& string1Start, const basic_iterator01 string1End,
				const basic_iterator01 stringStart, scan_state& scanState) noexcept {
				using simd_list_local					 = type_list_element_t<index, simd::avx_integer_list>;
				using integer_type						 = typename simd_list_local::integer_type;
				static constexpr auto avx_type			 = simd_list_local::type::avx_type;
				using simd_type_local					 = typename simd_type_wrapper<avx_type>::type;
				static constexpr uint64_t bytesProcessed = simd_list_local::bytesProcessed;
				if (scanState.complete) {
					return nullptr;
				}
				const simd_type_local simdValues00 = simd::gatherValue<simd_type_local>('\\');
				const simd_type_local simdValues01 = simd::gatherValue<simd_type_local>('"');
				const simd_type_local simdValues02 = simd::gatherValue<simd_type_local>(static_cast<char>(32));
				const simd_type_local simdValues03 = simd::gatherValue<simd_type_local>(static_cast<char>(0x80));
				const auto stringEndNew			   = string1End - bytesProcessed;
				while (string1Start < stringEndNew) {
					const simd_type_local simdValue = simd::gatherValuesU<simd_type_local>(string1Start);
					const integer_type delimiters =
						static_cast<integer_type>(simd::opBitMask(simd::opOr(simd::opCmpEqRaw(simdValue, simdValues00), simd::opCmpEqRaw(simdValue, simdValues01))));
					const integer_type controls = static_cast<integer_type>(simd::opBitMask(simd::opCmpLtRaw(simdValue, simdValues02)));
					const integer_type nonAscii = static_cast<integer_type>(~static_cast<integer_type>(simd::opBitMask(simd::opCmpLtRaw(simdValue, simdValues03))));
					if (delimiters == static_cast<integer_type>(0)) {
						scanState.result.control |= controls != static_cast<integer_type>(0);
						scanState.result.nonAscii |= nonAscii != static_cast<integer_type>(0);
						string1Start += bytesProcessed;
						continue;
					}
					const uint64_t offset		 = static_cast<uint64_t>(simd::countrZeroUnsafe(delimiters));
					const integer_type preceding = static_cast<integer_type>((static_cast<integer_type>(1) << offset) - static_cast<integer_type>(1));
					scanState.result.control |= (controls & preceding) != static_cast<integer_type>(0);
					scanState.result.nonAscii |= (nonAscii & preceding) != static_cast<integer_type>(0);
					string1Start += offset;
					if (*string1Start == '"') {
						scanState.result.length = static_cast<uint64_t>(string1Start - stringStart);
						scanState.result.found	= true;
						scanState.complete		= true;
						return nullptr;
					}
					scanState.result.escaped = true;
					string1Start += 2;
				}

				if constexpr (index == 2) {
					scanState.complete = true;
					while (string1Start < string1End) {
						const uint8_t currentChar = static_cast<uint8_t>(*string1Start);
						if (currentChar == uint_c_v<'"'>) {
							scanState.result.length = static_cast<uint64_t>(string1Start - stringStart);
							scanState.result.found	= true;
							return nullptr;
						}
						if (currentChar == uint_c_v<'\\'>) {
							scanState.result.escaped = true;
							string1Start += 2;
							continue;
						}
						scanState.result.control |= currentChar < 32;
						scanState.result.nonAscii |= currentChar >= 0x80;
						++string1Start;
					}
					return nullptr;
				} else {
					return string1Start;
				}
			}
		};

		template<typename basic_iterator01> JSONIFIER_INLINE static prescan_result impl(basic_iterator01 string1Start, const basic_iterator01 string1End) noexcept {
			const auto stringStart = string1Start;
			scan_state scanState{};
			string_parse_executor<string_prescan_step, make_ascending_range<startIndex, listSize>>::impl(string1Start, string1End, stringStart, scanState);
			return scanState.result;
		}
	};

	template<parse_options options> struct string_validating_copier {
		struct copy_state {
			utf8_validation_state validationState{};
			bool complete{};
			bool failed{};
		};

		struct string_copy_step {
			template<uint64_t index, typename basic_iterator01, typename basic_iterator02> JSONIFIER_INLINE static basic_iterator01 impl(basic_iterator01& string1Start,
				const basic_iterator01 string1End, basic_iterator02& string2, copy_state& copyState) noexcept {
				using simd_list_local					 = type_list_element_t<index, simd::avx_integer_list>;
				using integer_type						 = typename simd_list_local::integer_type;
				static constexpr auto avx_type			 = simd_list_local::type::avx_type;
				using simd_type_local					 = typename simd_type_wrapper<avx_type>::type;
				static constexpr uint64_t bytesProcessed = simd_list_local::bytesProcessed;
				if (copyState.complete) {
					return nullptr;
				}
				utf8_register_validator<simd_type_wrapper<avx_type>> validator{ copyState.validationState };
				const simd_type_local simdValues00 = simd::gatherValue<simd_type_local>(static_cast<char>(32));
				while (static_cast<uint64_t>(string1End - string1Start) >= bytesProcessed) {
					const simd_type_local simdValue = simd::gatherValuesU<simd_type_local>(string1Start);
					if (static_cast<integer_type>(simd::opBitMask(simd::opCmpLtRaw(simdValue, simdValues00))) != static_cast<integer_type>(0)) [[unlikely]] {
						copyState.failed   = true;
						copyState.complete = true;
						return nullptr;
					}
					validator.checkRegister(simdValue);
					simd::storeU(simdValue, string2);
					string1Start += bytesProcessed;
					string2 += bytesProcessed;
				}
				validator.flush();

				if constexpr (index == 2) {
					copyState.complete		 = true;
					const uint64_t remaining = static_cast<uint64_t>(string1End - string1Start);
					for (uint64_t x = 0; x < remaining; ++x) {
						if (static_cast<uint8_t>(string1Start[x]) < 32) [[unlikely]] {
							copyState.failed = true;
							return nullptr;
						}
					}
					validator.checkPartial(string1Start, remaining);
					if (validator.errors()) [[unlikely]] {
						copyState.failed = true;
						return nullptr;
					}
					jsonifierMemcpy(string2, string1Start, remaining);
					return nullptr;
				} else {
					return string1Start;
				}
			}
		};

		template<typename basic_iterator01, typename basic_iterator02>
		JSONIFIER_INLINE static bool impl(basic_iterator01 string1Start, uint64_t length, basic_iterator02 string2) noexcept {
			const auto string1End = string1Start + length;
			copy_state copyState{};
			string_parse_executor<string_copy_step, make_ascending_range<startIndex, listSize>>::impl(string1Start, string1End, string2, copyState);
			return !copyState.failed;
		}
	};

	consteval array<array<char, 8ULL>, 256ULL> genEscapeTable() noexcept {
		array<array<char, 8ULL>, 256ULL> returnValues{};
		constexpr char hex[]{ "0123456789ABCDEF" };
		for (uint64_t x = 0; x < 32ULL; ++x) {
			returnValues[x] = { { '\\', 'u', '0', '0', hex[x >> 4], hex[x & 0xF], '\0', 6 } };
		}
		auto setShort = [&](char value, char escaped) {
			returnValues[static_cast<uint8_t>(value)] = { { '\\', escaped, '\0', '\0', '\0', '\0', '\0', 2 } };
		};
		setShort('\b', 'b');
		setShort('\t', 't');
		setShort('\n', 'n');
		setShort('\f', 'f');
		setShort('\r', 'r');
		setShort('"', '"');
		setShort('\\', '\\');
		return returnValues;
	}

	alignas(64) inline constexpr auto escapeTable{ genEscapeTable() };

	template<serialize_options options> struct string_serializer {
		struct string_serialize_step {
			template<uint64_t index, typename basic_iterator01, typename basic_iterator02>
			JSONIFIER_INLINE static auto* impl(basic_iterator01& string1Start, const basic_iterator01 string1End, basic_iterator02& string2) noexcept {
				using simd_list_local					 = type_list_element_t<index, simd::avx_integer_list>;
				using integer_type						 = typename simd_list_local::integer_type;
				using simd_type							 = typename simd_list_local::type::type;
				static constexpr uint64_t bytesProcessed = simd_list_local::bytesProcessed;
				static constexpr uint64_t bitsPerByte	 = sizeof(integer_type) * 8 / bytesProcessed;
				static constexpr uint64_t laneCount		 = bytesProcessed / 16;
				static constexpr uint64_t laneBitCount	 = bitsPerByte * 16;
				static constexpr integer_type laneMask	 = static_cast<integer_type>(std::numeric_limits<integer_type>::max() >> (sizeof(integer_type) * 8 - laneBitCount));

				const auto stringEndNew		 = string1End - bytesProcessed;
				const simd_type simdValues01 = simd::gatherValue<simd_type>('"');
				const simd_type simdValues02 = simd::gatherValue<simd_type>('\\');
				const simd_type simdValues03 = simd::gatherValue<simd_type>(static_cast<char>(32));
				while (string1Start <= stringEndNew) {
					const simd_type simdValue	  = simd::gatherValuesU<simd_type>(string1Start);
					const integer_type escapeMask = escapeBits<integer_type>(simdValue, simdValues01, simdValues02, simdValues03);
					if (escapeMask == 0) [[likely]] {
						simd::storeU(simdValue, string2);
						string2 += bytesProcessed;
						string1Start += bytesProcessed;
					} else {
						if constexpr (laneCount > 0) {
							const integer_type laneEscapes = static_cast<integer_type>(static_cast<integer_type>(escapeMask) & laneMask);
							string2						   = escapeLane<bitsPerByte>(string1Start, string1Start + 16, string2, laneEscapes);
						}
						if constexpr (laneCount > 1) {
							const integer_type laneEscapes = static_cast<integer_type>(static_cast<integer_type>(escapeMask >> (laneBitCount * 1)) & laneMask);
							string2						   = escapeLane<bitsPerByte>(string1Start, string1Start + 16, string2, laneEscapes);
						}
						if constexpr (laneCount > 2) {
							const integer_type laneEscapes = static_cast<integer_type>(static_cast<integer_type>(escapeMask >> (laneBitCount * 2)) & laneMask);
							string2						   = escapeLane<bitsPerByte>(string1Start, string1Start + 16, string2, laneEscapes);
						}
						if constexpr (laneCount > 3) {
							const integer_type laneEscapes = static_cast<integer_type>(static_cast<integer_type>(escapeMask >> (laneBitCount * 3)) & laneMask);
							string2						   = escapeLane<bitsPerByte>(string1Start, string1Start + 16, string2, laneEscapes);
						}
					}
				}
				return string1Start;
			}
		};

		static constexpr uint64_t pageSize = 4096;

		// Sampled from Dr. Lemire's library, simdjson: https://github.com/simdjson/simdjson
		template<typename basic_iterator01, typename basic_iterator02>
		JSONIFIER_INLINE static basic_iterator02 overlappedFinish(basic_iterator01& string1Start, const basic_iterator01 string1End, basic_iterator02 string2) noexcept {
			const uint64_t remaining = static_cast<uint64_t>(string1End - string1Start);
			if (remaining == 0) {
				return string2;
			}

			using simd_list_local				  = type_list_element_t<2, simd::avx_integer_list>;
			using integer_type					  = typename simd_list_local::integer_type;
			using simd_type						  = typename simd_list_local::type::type;
			static constexpr uint64_t bitsPerByte = sizeof(integer_type) * 8 / 16;
			const simd_type simdValue			  = simd::gatherValuesU<simd_type>(string1End - 16);
			const simd_type simdValues01		  = simd::gatherValue<simd_type>('"');
			const simd_type simdValues02		  = simd::gatherValue<simd_type>('\\');
			const simd_type simdValues03		  = simd::gatherValue<simd_type>(static_cast<char>(32));
			const integer_type escapeMask =
				static_cast<integer_type>(escapeBits<integer_type>(simdValue, simdValues01, simdValues02, simdValues03) >> (bitsPerByte * (16 - remaining)));
			if (escapeMask == 0) [[likely]] {
				copyUpTo16(string2, string1Start, remaining);
				string1Start = string1End;
				return string2 + remaining;
			}
			return escapeLane<bitsPerByte>(string1Start, string1End, string2, escapeMask);
		}

		template<uint64_t bitsPerByte, typename basic_iterator01, typename basic_iterator02, typename integer_type> JSONIFIER_INLINE static basic_iterator02 escapeLane(
			basic_iterator01& string1Start, const basic_iterator01 laneEnd, basic_iterator02 string2, integer_type escapeMask) noexcept {
			while (escapeMask != 0) {
				const uint64_t runLength = simd::postCmpTzcntUnsafe(escapeMask);
				copyUpTo16(string2, string1Start, runLength);
				string2 += runLength;
				string1Start += runLength;
				const auto& entry = escapeTable[static_cast<uint8_t>(*string1Start)];
				pow2MemcpyWrapper<8>(string2, entry.data());
				string2 += static_cast<uint8_t>(entry[7]);
				++string1Start;
				escapeMask = static_cast<integer_type>(static_cast<integer_type>(escapeMask >> (bitsPerByte * runLength)) >> bitsPerByte);
			}
			const uint64_t trailing = static_cast<uint64_t>(laneEnd - string1Start);
			copyUpTo16(string2, string1Start, trailing);
			string1Start = laneEnd;
			return string2 + trailing;
		}

		template<typename integer_type, typename simd_type> JSONIFIER_INLINE static integer_type escapeBits(const simd_type& simdValue, const simd_type& simdValues01,
			const simd_type& simdValues02, const simd_type& simdValues03) noexcept {
			const auto flagged =
				simd::opOr(simd::opOr(simd::opCmpLtRaw(simdValue, simdValues03), simd::opCmpEqRaw(simdValue, simdValues02)), simd::opCmpEqRaw(simdValue, simdValues01));
			return static_cast<integer_type>(simd::opBitMaskRaw(flagged));
		}

		template<typename basic_iterator01, typename basic_iterator02>
		JSONIFIER_INLINE static basic_iterator02 scalarImpl(basic_iterator01& string1Start, const basic_iterator01 string1End, basic_iterator02 string2) noexcept {
			for (; string1Start < string1End; ++string1Start) {
				const uint8_t nextChar = static_cast<uint8_t>(*string1Start);
				const auto& entry	   = escapeTable[nextChar];
				if (const uint8_t escapeSize = static_cast<uint8_t>(entry[7]); escapeSize) {
					pow2MemcpyWrapper<8>(string2, entry.data());
					string2 += escapeSize;
				} else {
					*string2 = nextChar;
					++string2;
				}
			}
			return string2;
		}

		template<typename basic_iterator01, typename basic_iterator02>
		JSONIFIER_INLINE static basic_iterator02 swarFinish(basic_iterator01& string1Start, const basic_iterator01 string1End, basic_iterator02 string2) noexcept {
			while (string1End - string1Start >= 8) {
				uint64_t word;
				pow2MemcpyWrapper<8>(&word, string1Start);
				pow2MemcpyWrapper<8>(string2, &word);
				if constexpr (std::endian::native == std::endian::big) {
					word = internal::byteswap(word);
				}
				const uint64_t flags = flagMask(word);
				if (!flags) {
					string1Start += 8;
					string2 += 8;
					continue;
				}
				const uint64_t offset = static_cast<uint64_t>(simd::countrZeroUnsafe(flags)) >> 3;
				string1Start += offset;
				string2 += offset;
				const auto& entry = escapeTable[static_cast<uint8_t>(*string1Start)];
				pow2MemcpyWrapper<8>(string2, entry.data());
				string2 += static_cast<uint8_t>(entry[7]);
				++string1Start;
			}
			const uint64_t remaining = static_cast<uint64_t>(string1End - string1Start);
			if (remaining == 0) {
				return string2;
			}
			if constexpr (std::endian::native == std::endian::little) {
				uint64_t word;
				pow2MemcpyWrapper<8>(&word, string1End - 8);
				const uint64_t shift = (8 - remaining) * 8;
				word >>= shift;
				const uint64_t flags = flagMask(word) & (~uint64_t{ 0 } >> shift);
				if (!flags) {
					pow2MemcpyWrapper<8>(string2, &word);
					string1Start = string1End;
					return string2 + remaining;
				}
			}
			return scalarImpl(string1Start, string1End, string2);
		}

		template<typename basic_iterator01, typename basic_iterator02>
		JSONIFIER_INLINE static basic_iterator02 pageSafeShortImpl(basic_iterator01& string1Start, const uint64_t length, basic_iterator02 string2) noexcept {
			using simd_list_local				  = type_list_element_t<2, simd::avx_integer_list>;
			using integer_type					  = typename simd_list_local::integer_type;
			using simd_type						  = typename simd_list_local::type::type;
			static constexpr uint64_t bitsPerByte = sizeof(integer_type) * 8 / 16;
			const simd_type simdValue			  = loadPageSafe16<simd_type>(&*string1Start);
			const simd_type simdValues01		  = simd::gatherValue<simd_type>('"');
			const simd_type simdValues02		  = simd::gatherValue<simd_type>('\\');
			const simd_type simdValues03		  = simd::gatherValue<simd_type>(static_cast<char>(32));
			const integer_type lengthMask		  = static_cast<integer_type>((uint64_t{ 1 } << (bitsPerByte * length)) - 1);
			const integer_type escapeMask		  = static_cast<integer_type>(escapeBits<integer_type>(simdValue, simdValues01, simdValues02, simdValues03) & lengthMask);
			if (escapeMask == 0) [[likely]] {
				simd::storeU(simdValue, string2);
				string1Start += length;
				return string2 + length;
			}
			return escapeLane<bitsPerByte>(string1Start, string1Start + length, string2, escapeMask);
		}

		template<uint64_t index, typename basic_iterator01, typename basic_iterator02>
		JSONIFIER_INLINE static bool copyIfClean(basic_iterator01 string1Start, basic_iterator02 string2, const uint64_t length) noexcept {
			using simd_list_local					 = type_list_element_t<index, simd::avx_integer_list>;
			using integer_type						 = typename simd_list_local::integer_type;
			using simd_type							 = typename simd_list_local::type::type;
			static constexpr uint64_t bytesProcessed = simd_list_local::bytesProcessed;
			const simd_type simdValues01			 = simd::gatherValue<simd_type>('"');
			const simd_type simdValues02			 = simd::gatherValue<simd_type>('\\');
			const simd_type simdValues03			 = simd::gatherValue<simd_type>(static_cast<char>(32));
			const simd_type head					 = simd::gatherValuesU<simd_type>(string1Start);
			const simd_type tail					 = simd::gatherValuesU<simd_type>(string1Start + (length - bytesProcessed));
			const integer_type escapeMask			 = static_cast<integer_type>(
				escapeBits<integer_type>(head, simdValues01, simdValues02, simdValues03) | escapeBits<integer_type>(tail, simdValues01, simdValues02, simdValues03));
			if (escapeMask != 0) [[unlikely]] {
				return false;
			}
			simd::storeU(head, string2);
			simd::storeU(tail, string2 + (length - bytesProcessed));
			return true;
		}

		template<typename basic_iterator01, typename basic_iterator02>
		JSONIFIER_INLINE static basic_iterator02 smallImpl(basic_iterator01& string1Start, const uint64_t length, basic_iterator02 string2) noexcept {
			if (length >= 4) {
				uint32_t lo;
				uint32_t hi;
				pow2MemcpyWrapper<4>(&lo, string1Start);
				pow2MemcpyWrapper<4>(&hi, string1Start + length - 4);
				uint64_t word = static_cast<uint64_t>(lo) | (static_cast<uint64_t>(hi) << 32);
				if constexpr (std::endian::native == std::endian::big) {
					word = internal::byteswap(word);
				}
				if (!flagMask(word)) {
					pow2MemcpyWrapper<4>(string2, &lo);
					pow2MemcpyWrapper<4>(string2 + length - 4, &hi);
					string1Start += length;
					return string2 + length;
				}
			} else if (length >= 2) {
				uint16_t lo;
				uint16_t hi;
				pow2MemcpyWrapper<2>(&lo, string1Start);
				pow2MemcpyWrapper<2>(&hi, string1Start + length - 2);
				uint64_t word = static_cast<uint64_t>(lo) | (static_cast<uint64_t>(hi) << 16) | 0x8080808000000000ULL;
				if constexpr (std::endian::native == std::endian::big) {
					word = internal::byteswap(word);
				}
				if (!flagMask(word)) {
					pow2MemcpyWrapper<2>(string2, &lo);
					pow2MemcpyWrapper<2>(string2 + length - 2, &hi);
					string1Start += length;
					return string2 + length;
				}
			} else if (length == 1) {
				const uint8_t nextChar = static_cast<uint8_t>(*string1Start);
				if (escapeTable[nextChar][7] == 0) {
					*string2 = nextChar;
					++string1Start;
					return string2 + 1;
				}
			}
			const basic_iterator01 string1End = string1Start + length;
			return scalarImpl(string1Start, string1End, string2);
		}

#if JSONIFIER_ASAN_ENABLED
		template<typename simd_type> JSONIFIER_NOINLINE JSONIFIER_NO_SANITIZE_ADDRESS static simd_type loadPageSafe16(const void* source) noexcept {
	#if JSONIFIER_COMPILER_MSVC
			using unaligned_uint64 = uint64_t;
	#else
			using unaligned_uint64 [[gnu::may_alias, gnu::aligned(1)]] = uint64_t;
	#endif
			const unaligned_uint64* sourceNew = static_cast<const unaligned_uint64*>(source);
			const uint64_t values[2]{ sourceNew[0], sourceNew[1] };
			simd_type result;
			pow2MemcpyWrapper<16>(&result, values);
			return result;
		}
#else
		template<typename simd_type> JSONIFIER_INLINE static simd_type loadPageSafe16(const void* source) noexcept {
			return simd::gatherValuesU<simd_type>(source);
		}
#endif

		template<typename basic_iterator> JSONIFIER_INLINE static uint64_t escapedSize(basic_iterator stringStart, uint64_t length) noexcept {
			uint64_t requiredSize{};
			const auto stringEnd = stringStart + length;
			while (stringEnd - stringStart >= 8) {
				uint64_t word;
				pow2MemcpyWrapper<8>(&word, stringStart);
				if (!flagMask(word)) {
					requiredSize += 8;
				} else {
					for (uint64_t index{}; index < 8; ++index) {
						requiredSize += escapedByteSize(static_cast<uint8_t>(stringStart[index]));
					}
				}
				stringStart += 8;
			}
			for (; stringStart < stringEnd; ++stringStart) {
				requiredSize += escapedByteSize(static_cast<uint8_t>(*stringStart));
			}
			return requiredSize;
		}

		template<typename basic_iterator01, typename basic_iterator02>
		JSONIFIER_INLINE static basic_iterator02 impl(basic_iterator01 string1Start, basic_iterator02 string2, uint64_t lengthNew) noexcept {
			if (lengthNew < 16) {
				if ((static_cast<uint64_t>(std::bit_cast<uintptr_t>(&*string1Start)) & (pageSize - 1)) <= pageSize - 16) [[likely]] {
					return pageSafeShortImpl(string1Start, lengthNew, string2);
				}
				if (lengthNew < 8) {
					return smallImpl(string1Start, lengthNew, string2);
				}
				return swarFinish(string1Start, string1Start + lengthNew, string2);
			}
			if (lengthNew <= 64 && mediumImpl(string1Start, string2, lengthNew)) [[likely]] {
				return string2 + lengthNew;
			}
			const basic_iterator01 string1End = string1Start + lengthNew;
			string_parse_executor<string_serialize_step, make_ascending_range<startIndex, listSize>>::impl(string1Start, string1End, string2);
			return overlappedFinish(string1Start, string1End, string2);
		}

		template<typename basic_iterator01, typename basic_iterator02>
		JSONIFIER_INLINE static bool mediumImpl(basic_iterator01 string1Start, basic_iterator02 string2, const uint64_t length) noexcept {
			if (length <= 32) {
				return copyIfClean<2>(string1Start, string2, length);
			}
			if constexpr (startIndex <= 1) {
				return copyIfClean<1>(string1Start, string2, length);
			} else {
				return copyIfClean<2>(string1Start, string2, 32) && copyIfClean<2>(string1Start + (length - 32), string2 + (length - 32), 32);
			}
		}

		// Sampled from Dr. Lemire's library, simdjson: https://github.com/simdjson/simdjson
		template<typename basic_iterator01, typename basic_iterator02>
		JSONIFIER_INLINE static void copyUpTo16(basic_iterator02 destination, basic_iterator01 source, uint64_t length) noexcept {
			using value_type = remove_cvref_t<decltype(*destination)>;
			if (length >= 8) {
				pow2MemcpyWrapper<8>(destination, source);
				pow2MemcpyWrapper<8>(destination + length - 8, source + length - 8);
			} else if (length >= 4) {
				pow2MemcpyWrapper<4>(destination, source);
				pow2MemcpyWrapper<4>(destination + length - 4, source + length - 4);
			} else if (length > 0) {
				destination[0]			 = static_cast<value_type>(source[0]);
				destination[length >> 1] = static_cast<value_type>(source[length >> 1]);
				destination[length - 1]	 = static_cast<value_type>(source[length - 1]);
			}
		}

		JSONIFIER_INLINE static uint64_t escapedByteSize(uint8_t nextChar) noexcept {
			const uint64_t escapeSize = static_cast<uint8_t>(escapeTable[nextChar][7]);
			return escapeSize > 0 ? escapeSize : 1;
		}

		JSONIFIER_INLINE static uint64_t flagMask(uint64_t word) noexcept {
			static constexpr uint64_t lo7Mask  = 0x7F7F7F7F7F7F7F7FULL;
			static constexpr uint64_t highBits = 0x8080808080808080ULL;
			static constexpr uint64_t quotes   = 0x2222222222222222ULL;
			static constexpr uint64_t slashes  = 0x5C5C5C5C5C5C5C5CULL;
			static constexpr uint64_t bits56   = 0x6060606060606060ULL;
			const uint64_t lo7				   = word & lo7Mask;
			const uint64_t quote			   = (lo7 ^ quotes) + lo7Mask;
			const uint64_t backslash		   = (lo7 ^ slashes) + lo7Mask;
			const uint64_t less32			   = (word & bits56) + lo7Mask;
			return ~((quote & backslash & less32) | word) & highBits;
		}
	};

	template<string_literal string> consteval convert_length_to_int_t<string.size()> getStringAsInt() noexcept {
		const auto* stringNew = string.data();
		convert_length_to_int_t<string.size()> returnValue{};
		for (uint64_t x = 0; x < string.size(); ++x) {
			returnValue |= static_cast<convert_length_to_int_t<string.size()>>(stringNew[x]) << x * 8;
		}
		if constexpr (std::endian::native == std::endian::big) {
			returnValue = internal::byteswap(returnValue);
		}
		return returnValue;
	}

	template<string_literal stringNew> JSONIFIER_INLINE bool compareStringAsInt(read_buffer_ptr src) noexcept {
		using integer_type = convert_length_to_int_t<stringNew.size()>;
		static constexpr auto string{ stringNew };
		static_assert(stringNew.size() == 4, "Sorry, but please only use a string with a length of 4 in this function!");
		static constexpr auto stringInt{ getStringAsInt<string>() };
		integer_type sourceVal;
		pow2MemcpyWrapper<string.size()>(&sourceVal, src);
		return !static_cast<bool>(sourceVal ^ stringInt);
	}

	JSONIFIER_INLINE bool validateBool(read_buffer_ptr context, read_buffer_ptr end) noexcept {
		const auto remaining = end - context;
		if (remaining >= 4 && compareStringAsInt<"true">(context)) {
			return true;
		} else if (remaining >= 5 && compareStringAsInt<"fals">(context) && context[4] == uint_c_v<'e'>) {
			return true;
		}
		return false;
	}

	JSONIFIER_INLINE bool validateNull(read_buffer_ptr context, read_buffer_ptr end) noexcept {
		if (end - context >= 4 && compareStringAsInt<"null">(context)) [[likely]] {
			return true;
		} else {
			return false;
		}
	}

}
