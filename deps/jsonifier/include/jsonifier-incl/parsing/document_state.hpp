/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/parsing/document_state.hpp
 */
#pragma once

#include <jsonifier-incl/parsing/field_index_map.hpp>
#include <jsonifier-incl/parsing/string_parser.hpp>

namespace jsonifier::internal {

	struct document_state {
		static constexpr uint8_t reverseEngageScore{ 4 };
		static constexpr uint8_t reverseMaxScore{ 8 };

		JSONIFIER_INLINE generic::error_codes advanceChild(read_structural_index_ptr childValue, uint64_t containerDepth, uint8_t closer,
			read_structural_index_ptr& next) noexcept {
			const read_buffer_ptr base		   = root;
			read_structural_index_ptr limit	   = tapeEnd;
			read_structural_index_ptr position = cursor;
			uint64_t currentDepth			   = depth;
			if (currentDepth == containerDepth) {
				if (position <= childValue) {
					position = skipValue(base, childValue, limit);
				}
			} else if (currentDepth > containerDepth) {
				position = finishToDepth(base, position, limit, currentDepth, containerDepth);
			}
			generic::error_codes error_code{ generic::error_codes::tape_error };
			if (position >= limit) [[unlikely]] {
				error_code = generic::error_codes::unclosed_container;
			} else if (const uint8_t separator = base[*position]; separator == uint_c_v<','>) [[likely]] {
				++position;
				next	   = position;
				error_code = position < limit ? generic::error_codes::success : generic::error_codes::unclosed_container;
			} else if (separator == closer) [[likely]] {
				++position;
				--currentDepth;
				next	   = nullptr;
				error_code = generic::error_codes::success;
			}
			cursor = position;
			depth  = currentDepth;
			return error_code;
		}

		JSONIFIER_INLINE static unrestrict_t<read_structural_index_ptr> finishToDepth(read_buffer_ptr base, read_structural_index_ptr position, read_structural_index_ptr limit,
			uint64_t& currentDepth, uint64_t targetDepth) noexcept {
			int64_t localDepth{ static_cast<int64_t>(currentDepth) };
			const int64_t target{ static_cast<int64_t>(targetDepth) };
#if JSONIFIER_PLATFORM_LINUX && JSONIFIER_COMPILER_GCC
			while (localDepth > target && position < limit) {
				localDepth += depthDeltaTable[base[*position]];
				++position;
			}
#else
			if (localDepth > target) {
				while (position < limit) {
					localDepth += depthDeltaTable[base[*position]];
					++position;
					if (localDepth <= target) {
						break;
					}
				}
			}
#endif
			currentDepth = static_cast<uint64_t>(localDepth);
			return position;
		}

		template<bool minified> JSONIFIER_INLINE bool keyMatches(read_structural_index_ptr iter, string_view key) const noexcept {
			const uint64_t keySize	 = key.size();
			const uint64_t span		 = static_cast<uint64_t>(*(iter + 1) - *iter);
			read_buffer_ptr keyStart = root + *iter + 1;
			if (span == keySize + 2) [[likely]] {
				return internal::comparison::compare(keyStart, key.data(), keySize);
			}
			if constexpr (minified) {
				return false;
			} else {
				return span > keySize + 2 && keyStart[keySize] == uint_c_v<'"'> && internal::comparison::compare(keyStart, key.data(), keySize);
			}
		}

		JSONIFIER_INLINE static unrestrict_t<read_structural_index_ptr> skipValue(read_buffer_ptr base, read_structural_index_ptr iter, read_structural_index_ptr limit) noexcept {
			if (!isOpener(base[*iter])) [[likely]] {
				return iter + 1;
			}
			int64_t openCount{ 1 };
			++iter;
			while (iter < limit) {
				openCount += depthDeltaTable[base[*iter]];
				++iter;
				if (openCount == 0) {
					return iter;
				}
			}
			return limit;
		}

		JSONIFIER_INLINE bool keyHasEscape(read_structural_index_ptr iter) const noexcept {
			read_buffer_ptr keyStart = root + *iter + 1;
			read_buffer_ptr keyLimit = root + *(iter + 1);
			return keyLimit > keyStart && char_comparison<'\\', uint8_t>::memchar(keyStart, static_cast<uint64_t>(keyLimit - keyStart));
		}

		JSONIFIER_INLINE unrestrict_t<read_structural_index_ptr> skipValue(read_structural_index_ptr iter) const noexcept {
			return skipValue(root, iter, tapeEnd);
		}

		template<bool minified> JSONIFIER_INLINE string_view rawKey(read_structural_index_ptr iter) const noexcept {
			read_buffer_ptr keyStart = root + *iter + 1;
			read_buffer_ptr keyEnd	 = root + *(iter + 1) - 1;
			if constexpr (!minified) {
				while (keyEnd > keyStart && *keyEnd != uint_c_v<'"'>) {
					--keyEnd;
				}
			}
			return string_view{ std::bit_cast<const char*>(keyStart), static_cast<uint64_t>(keyEnd - keyStart) };
		}

		JSONIFIER_INLINE bool unescapedKeyMatches(read_structural_index_ptr iter, string_view key) const noexcept {
			read_buffer_ptr keyStart = root + *iter + 1;
			char* unescaped			 = arena->scratch();
			const auto scanned		 = string_parser::impl(keyStart, end, unescaped);
			return scanned.outLength == key.size() && internal::comparison::compare(unescaped, key.data(), key.size());
		}

		template<typename word_type> JSONIFIER_INLINE static word_type loadWord(read_buffer_ptr source) noexcept {
			word_type word;
			pow2MemcpyWrapper<sizeof(word_type)>(&word, source);
			return word;
		}

		template<uint8_t value> JSONIFIER_INLINE bool charAt(read_structural_index_ptr iter) const noexcept {
			return root[*iter] == value;
		}

		JSONIFIER_INLINE uint8_t charAt(read_structural_index_ptr iter) const noexcept {
			return root[*iter];
		}

		JSONIFIER_INLINE void finishToDepth(uint64_t targetDepth) noexcept {
			uint64_t currentDepth{ depth };
			cursor = finishToDepth(root, cursor, tapeEnd, currentDepth, targetDepth);
			depth  = currentDepth;
		}

		JSONIFIER_INLINE void noteBackwardHit() noexcept {
			reverseScore += static_cast<uint8_t>(reverseScore < reverseMaxScore);
		}

		JSONIFIER_INLINE static bool isOpener(uint8_t c) noexcept {
			return (c | 0x20) == uint_c_v<'{'>;
		}

		JSONIFIER_INLINE static bool isCloser(uint8_t c) noexcept {
			return (c | 0x20) == uint_c_v<'}'>;
		}

		JSONIFIER_INLINE void noteForwardHit() noexcept {
			reverseScore -= static_cast<uint8_t>(reverseScore > 0);
		}

		JSONIFIER_INLINE bool reverseMode() const noexcept {
			return reverseScore >= reverseEngageScore;
		}

		read_structural_index_ptr tapeEnd{};
		read_structural_index_ptr cursor{};
		read_structural_index_ptr tape{};
		field_index_map* fieldIndex{};
		read_buffer_ptr root{};
		read_buffer_ptr end{};
		string_arena* arena{};
		uint64_t depth{};
		uint8_t reverseScore{};
	};

}
