/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/simd/jsonifier_cpu_properties.hpp.in
 */
#pragma once

#include <jsonifier-incl/core/defines.hpp>
#include <cstdint>

#if !defined(JSONIFIER_CPU_ALIGNMENT)
	#define JSONIFIER_CPU_ALIGNMENT 32
#endif

namespace jsonifier::internal {

	enum class cpu_property_types : uint64_t {
		alignment,
		arg_alignment,
	};

	struct cpu_properties {
	  public:
		static constexpr uint64_t values[]{ JSONIFIER_CPU_ALIGNMENT, 64ULL };

		static consteval uint64_t getValue(cpu_property_types index) {
			return values[static_cast<uint64_t>(index)];
		}
	};

}
