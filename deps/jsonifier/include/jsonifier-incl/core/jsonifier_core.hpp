/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/core/jsonifier_core.hpp
 */
#pragma once

#include <jsonifier-incl/simd/simd_config.hpp>
#include <jsonifier-incl/simd/avx.hpp>
#include <jsonifier-incl/simd/neon.hpp>
#include <jsonifier-incl/simd/sve2.hpp>
#include <jsonifier-incl/simd/fallback.hpp>
#include <jsonifier-incl/simd/avx_stage1.hpp>
#include <jsonifier-incl/simd/neon_stage1.hpp>
#include <jsonifier-incl/simd/sve2_stage1.hpp>
#include <jsonifier-incl/simd/add_tape_values.hpp>
#include <jsonifier-incl/simd/utf8_validation.hpp>
#include <jsonifier-incl/utilities/simd.hpp>
#include <jsonifier-incl/utilities/string_utils.hpp>
#include <jsonifier-incl/parsing/generic_iterator.hpp>

namespace jsonifier {

	class jsonifier_core : public internal::generic_iterator<jsonifier_core> {
	  public:
		friend struct internal::generic_iterator<jsonifier_core>;

		inline jsonifier_core() noexcept = default;

		inline jsonifier_core& operator=(jsonifier_core&& other) noexcept {
			if (this != &other) [[likely]] {
				podSection	= internal::move(other.podSection);
				section		= internal::move(other.section);
				genericData = internal::move(other.genericData);
			}
			return *this;
		}

		inline jsonifier_core(jsonifier_core&& other) noexcept
			: podSection{ internal::move(other.podSection) }, section{ internal::move(other.section) }, genericData{ internal::move(other.genericData) } {
		}

		jsonifier_core& operator=(const jsonifier_core& other) = delete;
		jsonifier_core(const jsonifier_core& other)			   = delete;

		template<parse_options options = parse_options{ .newLineDelimited = true }, typename buffer_type>
		inline generic::document_stream<jsonifier_core, options> iterateMany(const buffer_type& in, uint64_t batchSize = options.defaultBatchSize) noexcept {
			return generic::document_stream<jsonifier_core, options>{ *this, std::bit_cast<internal::read_buffer_ptr>(in.data()), in.size(), batchSize };
		}

		inline ~jsonifier_core() noexcept = default;

	  protected:
		template<typename, parse_options> friend class generic::document_stream;

		JSONIFIER_INLINE internal::generic_storage& genericStorage() noexcept {
			return genericData;
		}

		internal::pod_simd_string_reader podSection{};
		internal::simd_string_reader section{};
		internal::generic_storage genericData{};
	};

}
