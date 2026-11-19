/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/parsing/generic_document.hpp
 */
#pragma once

#include <jsonifier-incl/parsing/generic_object.hpp>
#include <jsonifier-incl/parsing/generic_array.hpp>

namespace jsonifier::generic {

	template<bool minified> class generic_document : public generic_value<minified> {
	  public:
		using generic_value<minified>::generic_value;

		JSONIFIER_INLINE bool atEnd() const noexcept {
			return this->error_code == error_codes::success && this->doc->depth == 0 && this->doc->cursor == this->doc->tapeEnd;
		}
	};

}
