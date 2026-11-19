/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/parsing/generic_common_post.hpp
 */
#pragma once

#include <jsonifier-incl/parsing/generic_common_pre.hpp>

namespace jsonifier::internal {

	struct generic_storage {
		JSONIFIER_INLINE generic_storage() noexcept = default;

		JSONIFIER_INLINE generic_storage& operator=(generic_storage&& other) noexcept {
			if (this != &other) [[likely]] {
				fieldIndex	= internal::move(other.fieldIndex);
				arena		= internal::move(other.arena);
				state		= document_state{};
				other.state = document_state{};
			}
			return *this;
		}

		JSONIFIER_INLINE generic_storage(generic_storage&& other) noexcept : fieldIndex{ internal::move(other.fieldIndex) }, arena{ internal::move(other.arena) } {
			other.state = document_state{};
		}

		generic_storage& operator=(const generic_storage&) = delete;
		generic_storage(const generic_storage&)			   = delete;

		JSONIFIER_INLINE document_state& stateFor() noexcept {
			return state;
		}

		field_index_map fieldIndex{};
		document_state state{};
		string_arena arena{};
	};

}

namespace jsonifier::generic {

	template<bool minified> JSONIFIER_INLINE generic_object<minified> generic_value<minified>::getObject() const noexcept {
		if (error_code != error_codes::success) [[unlikely]] {
			return object{ error_code };
		}
		if (!doc->template charAt<internal::uint_c_v<'{'>>(iter)) [[unlikely]] {
			return object{ error_codes::incorrect_type };
		}
		doc->cursor = iter + 1;
		doc->depth	= depth + 1;
		return object{ doc, iter, depth + 1 };
	}

	template<bool minified> JSONIFIER_INLINE generic_array<minified> generic_value<minified>::getArray() const noexcept {
		if (error_code != error_codes::success) [[unlikely]] {
			return array{ error_code };
		}
		if (!doc->template charAt<internal::uint_c_v<'['>>(iter)) [[unlikely]] {
			return array{ error_codes::incorrect_type };
		}
		doc->cursor = iter + 1;
		doc->depth	= depth + 1;
		return array{ doc, iter, depth + 1 };
	}

	template<bool minified> template<internal::string_view_convertible_types key_type>
	JSONIFIER_INLINE generic_value<minified> generic_value<minified>::operator[](key_type&& key) const noexcept {
		return getObject().findFieldUnordered(static_cast<string_view>(key));
	}

	template<bool minified> JSONIFIER_INLINE generic_value<minified> generic_value<minified>::operator[](uint64_t index) const noexcept {
		return getArray().at(index);
	}

	template<bool minified> JSONIFIER_INLINE bool generic_value<minified>::unescapePointerToken(string_view& token, string& unescapedToken) noexcept {
		if (!internal::char_comparison<'~', char>::memchar(token.data(), token.size())) {
			return true;
		}
		unescapedToken.clear();
		for (uint64_t x = 0; x < token.size(); ++x) {
			if (token[x] != '~') {
				unescapedToken.emplace_back(token[x]);
			} else if (x + 1 < token.size() && (token[x + 1] == '0' || token[x + 1] == '1')) {
				unescapedToken.emplace_back(token[x + 1] == '0' ? '~' : '/');
				++x;
			} else {
				return false;
			}
		}
		token = unescapedToken;
		return true;
	}

	template<bool minified> JSONIFIER_INLINE generic_value<minified> generic_value<minified>::findFieldUnordered(string_view key) const noexcept {
		return getObject().findFieldUnordered(key);
	}

	template<bool minified> JSONIFIER_INLINE error_codes generic_value<minified>::parsePointerIndex(string_view token, uint64_t& index) noexcept {
		if (token.empty() || (token.size() > 1 && token[0] == '0')) [[unlikely]] {
			return error_codes::invalid_json_pointer;
		}
		index = 0;
		for (const char digit: token) {
			if (digit < '0' || digit > '9') [[unlikely]] {
				return error_codes::invalid_json_pointer;
			}
			const uint64_t digitValue = static_cast<uint64_t>(digit - '0');
			if (index > (std::numeric_limits<uint64_t>::max() - digitValue) / 10) [[unlikely]] {
				return error_codes::index_out_of_bounds;
			}
			index = index * 10 + digitValue;
		}
		return error_codes::success;
	}

	template<bool minified> template<typename value_type> JSONIFIER_INLINE error_codes generic_value<minified>::get(value_type& out) const {
		if constexpr (std::is_same_v<value_type, double>) {
			return getDouble(out);
		} else if constexpr (std::is_same_v<value_type, int64_t>) {
			return getInt64(out);
		} else if constexpr (std::is_same_v<value_type, uint64_t>) {
			return getUint64(out);
		} else if constexpr (std::is_same_v<value_type, bool>) {
			return getBool(out);
		} else if constexpr (std::is_same_v<value_type, std::string_view> || internal::buffer_like_types<value_type>) {
			return getString(out);
		} else if constexpr (std::is_same_v<value_type, object>) {
			out = getObject();
			return out.error();
		} else if constexpr (std::is_same_v<value_type, array>) {
			out = getArray();
			return out.error();
		} else {
			static_assert(std::is_same_v<value_type, value>, "Unsupported generic::value::get type.");
			out = *this;
			return error_code;
		}
	}

	template<bool minified> template<typename element_type> JSONIFIER_INLINE error_codes generic_value<minified>::getArray(std::vector<element_type>& out) const {
		const array arrayView = getArray();
		if constexpr (internal::float_types<element_type>) {
			return arrayView.getFloats(out);
		} else {
			if (arrayView.error() != error_codes::success) [[unlikely]] {
				return arrayView.error();
			}
			out.clear();
			for (auto element: arrayView) {
				if constexpr (internal::vector_types<element_type>) {
					if (const error_codes element_error_code = element.getArray(out.emplace_back()); element_error_code != error_codes::success) [[unlikely]] {
						return element_error_code;
					}
				} else {
					if (const error_codes element_error_code = element.get(out.emplace_back()); element_error_code != error_codes::success) [[unlikely]] {
						return element_error_code;
					}
				}
			}
			return error_codes::success;
		}
	}

	template<bool minified> JSONIFIER_INLINE generic_value<minified> generic_value<minified>::findField(string_view key) const noexcept {
		return getObject().findField(key);
	}

	template<bool minified> JSONIFIER_INLINE generic_value<minified> generic_value<minified>::pointerStep(string_view token, string& unescapedToken) const noexcept {
		switch (doc->charAt(iter)) {
			case internal::uint_c_v<'{'>: {
				return unescapePointerToken(token, unescapedToken) ? findField(token) : value{ error_codes::invalid_json_pointer };
			}
			case internal::uint_c_v<'['>: {
				uint64_t index{};
				const error_codes index_error_code = parsePointerIndex(token, index);
				return index_error_code == error_codes::success ? getArray().at(index) : value{ index_error_code };
			}
			default: {
				return value{ error_codes::invalid_json_pointer };
			}
		}
	}

	template<bool minified> JSONIFIER_INLINE generic_value<minified> generic_value<minified>::atPointer(string_view pointer) const noexcept {
		value current{ *this };
		string unescapedToken{};
		const char* position   = pointer.data();
		const char* pointerEnd = position + pointer.size();
		while (current.error_code == error_codes::success && position < pointerEnd) {
			if (*position != '/') [[unlikely]] {
				current = value{ error_codes::invalid_json_pointer };
			} else {
				++position;
				const char* slash	 = internal::char_comparison<'/', char>::memchar(position, static_cast<uint64_t>(pointerEnd - position));
				const char* tokenEnd = slash ? slash : pointerEnd;
				current				 = current.pointerStep(string_view{ position, static_cast<uint64_t>(tokenEnd - position) }, unescapedToken);
				position			 = tokenEnd;
			}
		}
		return current;
	}

}
