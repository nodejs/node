/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/core/config.hpp
 */
#pragma once

#include <jsonifier-incl/core/defines.hpp>
#include <jsonifier-incl/simd/jsonifier_cpu_properties.hpp>

namespace jsonifier::internal {

	template<typename... arg_types> struct banned_reinterpret_cast {
		static_assert(sizeof...(arg_types) == 0,
			"Sorry, but reinterpret_cast is banned in this library! Only use our public-facing include <jsonifier> in your code! Or, if you're inside our own headers, remove "
			"the std library include you just added.");
	};

	template<typename... arg_types> struct banned_const_cast {
		static_assert(sizeof...(arg_types) == 0,
			"Sorry, but const_cast is banned in this library! Only use our public-facing include <jsonifier> in your code! Or, if you're inside our own headers, remove "
			"the std library include you just added.");
	};

	template<typename... arg_types> struct banned_dynamic_cast {
		static_assert(sizeof...(arg_types) == 0,
			"Sorry, but dynamic_cast is banned in this library! Only use our public-facing include <jsonifier> in your code! Or, if you're inside our own headers, remove "
			"the std library include you just added.");
	};

}

namespace jsonifier {

	struct serialize_options {
		uint64_t indentSize{ 3 };
		uint64_t indent{};
		char indentChar{ ' ' };
		bool prescanStrings{};
		bool prettify{};
	};

	struct parse_options {
		uint64_t defaultBatchSize{ 1024 * 1024 };
		uint64_t maxDepth{ 1024 };
		bool nullTerminated{ true };
		bool newLineDelimited{};
		bool partialRead{};
		bool knownOrder{};
		bool minified{};
	};

	struct prettify_options {
		uint64_t indentSize{ 3 };
		char indentChar{ ' ' };
	};

}
