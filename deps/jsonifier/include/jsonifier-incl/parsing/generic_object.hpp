/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/parsing/generic_object.hpp
 */
#pragma once

#include <jsonifier-incl/parsing/generic_field.hpp>

namespace jsonifier::generic {

	template<bool minified> class generic_object {
	  public:
		using document_state = internal::document_state;
		using field			 = generic_field<minified>;
		using value			 = generic_value<minified>;

		class iterator {
		  public:
			using iterator_category = std::forward_iterator_tag;
			using value_type		= field;
			using difference_type	= ptrdiff_t;

			JSONIFIER_INLINE iterator() noexcept = default;

			JSONIFIER_INLINE iterator(document_state* docNew, internal::read_structural_index_ptr iterNew, uint64_t depthNew) noexcept
				: iter{ iterNew }, doc{ docNew }, depth{ depthNew } {
				validateFieldStart();
			}

			JSONIFIER_INLINE iterator& operator++() noexcept {
				if (error_code != error_codes::success) [[unlikely]] {
					iter = nullptr;
					return *this;
				}
				error_code = doc->advanceChild(iter + 2, depth, internal::uint_c_v<'}'>, iter);
				validateFieldStart();
				return *this;
			}

			JSONIFIER_INLINE bool operator==(const iterator& other) const noexcept {
				return iter == other.iter;
			}

			JSONIFIER_INLINE field operator*() const noexcept {
				return field{ doc, iter, depth, error_code };
			}

		  protected:
			JSONIFIER_INLINE void validateFieldStart() noexcept {
				if (error_code == error_codes::success && iter &&
					(!doc->template charAt<internal::uint_c_v<'"'>>(iter) || iter + 1 >= doc->tapeEnd || !doc->template charAt<internal::uint_c_v<':'>>(iter + 1))) [[unlikely]] {
					error_code = error_codes::tape_error;
				}
			}

			internal::read_structural_index_ptr iter{};
			document_state* doc{};
			uint64_t depth{};
			error_codes error_code{ error_codes::success };
		};

#if JSONIFIER_PLATFORM_MAC && JSONIFIER_COMPILER_CLANG
		static constexpr uint32_t indexThreshold{ 16 };
#else
		static constexpr uint32_t indexThreshold{ 8 };
#endif
		static constexpr uint32_t wrapThreshold{ 2 };
		static constexpr uint32_t trailCapacity{ 16 };

		JSONIFIER_INLINE generic_object() noexcept = default;

		JSONIFIER_INLINE generic_object(document_state* docNew, internal::read_structural_index_ptr openNew, uint64_t depthNew) noexcept
			: open{ openNew }, doc{ docNew }, depth{ depthNew } {
		}

		JSONIFIER_INLINE generic_object(error_codes errorCodeNew) noexcept : error_code{ errorCodeNew } {
		}

		[[nodiscard]] JSONIFIER_INLINE error_codes countFields(uint64_t& out) const noexcept {
			if (error_code != error_codes::success) [[unlikely]] {
				return error_code;
			}
			out = 0;
			for (auto iter = begin(); iter != end(); ++iter) {
				if (const error_codes field_error_code = (*iter).error(); field_error_code != error_codes::success) [[unlikely]] {
					return field_error_code;
				}
				++out;
			}
			return error_codes::success;
		}

		JSONIFIER_INLINE iterator begin() const noexcept {
			if (error_code != error_codes::success) [[unlikely]] {
				return iterator{};
			}
			document_state* const state					 = doc;
			internal::read_structural_index_ptr position = open + 1;
			if (position < state->tapeEnd && state->template charAt<internal::uint_c_v<'}'>>(position)) {
				state->cursor = position + 1;
				state->depth  = depth - 1;
				return iterator{};
			}
			state->cursor = position;
			state->depth  = depth;
			if (position >= state->tapeEnd) [[unlikely]] {
				return iterator{};
			}
			return iterator{ state, position, depth };
		}

		[[nodiscard]] JSONIFIER_INLINE error_codes rawJson(string_view& out) const noexcept {
			return value{ doc, open, depth - 1 }.rawJson(out);
		}

		JSONIFIER_INLINE value atPointer(string_view pointer) const noexcept {
			return value{ doc, open, depth - 1 }.atPointer(pointer);
		}

		JSONIFIER_INLINE value findFieldUnordered(string_view key) noexcept {
			value found{ error_code };
			if (error_code == error_codes::success) [[likely]] {
				settle();
				found = expectedField(key);
				if (found.error_code == error_codes::no_such_field) [[unlikely]] {
					found = indexedField(key);
				}
			}
			return found;
		}

		JSONIFIER_INLINE value operator[](string_view key) noexcept {
			return findFieldUnordered(key);
		}

		JSONIFIER_INLINE value findField(string_view key) noexcept {
			value found{ error_code };
			if (error_code == error_codes::success) [[likely]] {
				settle();
				found = scanFields<false>(key, doc->tapeEnd);
				if (found.error_code == error_codes::no_such_field) [[unlikely]] {
					found = escapeAwareRetry(key, doc->cursor);
				}
			}
			return found;
		}

		JSONIFIER_INLINE error_codes error() const noexcept {
			return error_code;
		}

		JSONIFIER_INLINE iterator end() const noexcept {
			return iterator{};
		}

	  protected:
		JSONIFIER_INLINE void recordField(internal::read_structural_index_ptr keyIter) const noexcept {
			document_state* const state		   = doc;
			internal::read_buffer_ptr keyStart = state->root + *keyIter + 1;
			internal::read_buffer_ptr keyEnd   = state->root + *(keyIter + 1) - 1;
			if constexpr (!minified) {
				while (keyEnd > keyStart && *keyEnd != internal::uint_c_v<'"'>) {
					--keyEnd;
				}
			}
			const uint32_t object = objectIndex();
			state->fieldIndex->insert(internal::field_index_map::hashKey(keyStart, static_cast<uint64_t>(keyEnd - keyStart), object), object,
				static_cast<uint32_t>(keyIter - state->tape));
		}

		JSONIFIER_INLINE value expectedField(string_view key) const noexcept {
			document_state* const state					  = doc;
			internal::read_structural_index_ptr candidate = state->cursor;
			if (candidate < state->tapeEnd && state->template charAt<internal::uint_c_v<','>>(candidate)) {
				++candidate;
			}
			if (candidate + 1 < state->tapeEnd && state->template charAt<internal::uint_c_v<'"'>>(candidate) && state->template charAt<internal::uint_c_v<':'>>(candidate + 1) &&
				state->template keyMatches<minified>(candidate, key)) [[likely]] {
				state->cursor = candidate + 2;
				return value{ state, candidate + 2, depth };
			}
			return value{ error_codes::no_such_field };
		}

		JSONIFIER_INLINE value extendTrail(string_view key, internal::read_structural_index_ptr resumePoint, bool resumed) noexcept {
			document_state* const state					= doc;
			internal::read_structural_index_ptr tapeEnd = state->tapeEnd;
			internal::read_structural_index_ptr cursor	= trailCount == 0 ? open + 1 : state->skipValue(state->tape + trailEntry(trailCount - 1) + 2);
			while (cursor < tapeEnd) {
				const uint8_t current = state->charAt(cursor);
				if (current == internal::uint_c_v<'}'>) {
					trailComplete = true;
					return value{ error_codes::no_such_field };
				}
				if (current == internal::uint_c_v<','>) {
					++cursor;
					continue;
				}
				if (current == internal::uint_c_v<'"'> && cursor + 1 < tapeEnd && state->template charAt<internal::uint_c_v<':'>>(cursor + 1)) [[likely]] {
					if (trailCount == trailCapacity) {
						trailOverflow = true;
						return value{ error_codes::no_such_field };
					}
					const uint32_t keyOffset = static_cast<uint32_t>(cursor - state->tape);
					internal::pow2MemcpyWrapper<sizeof(uint32_t)>(trailBytes + trailCount * sizeof(uint32_t), &keyOffset);
					++trailCount;
					if (state->template keyMatches<minified>(cursor, key)) {
						return trailHit(cursor, resumePoint, resumed);
					}
					cursor += 2;
				}
				cursor = state->skipValue(cursor);
			}
			trailOverflow = true;
			return value{ error_codes::no_such_field };
		}

		JSONIFIER_INLINE void indexAllFields() const noexcept {
			document_state* const state					= doc;
			internal::read_structural_index_ptr cursor	= open + 1;
			internal::read_structural_index_ptr tapeEnd = state->tapeEnd;
			while (cursor < tapeEnd) {
				const uint8_t current = state->charAt(cursor);
				if (current == internal::uint_c_v<'}'>) {
					return;
				}
				if (current == internal::uint_c_v<','>) {
					++cursor;
					continue;
				}
				if (current == internal::uint_c_v<'"'> && cursor + 1 < tapeEnd && state->template charAt<internal::uint_c_v<':'>>(cursor + 1)) [[likely]] {
					recordField(cursor);
					cursor += 2;
				}
				cursor = state->skipValue(cursor);
			}
		}

		template<bool escapeAware> JSONIFIER_INLINE value scanFields(string_view key, internal::read_structural_index_ptr stop) noexcept {
			document_state* const state					= doc;
			internal::read_structural_index_ptr cursor	= state->cursor;
			internal::read_structural_index_ptr tapeEnd = state->tapeEnd;
			uint32_t skipped{ skippedKeys };
			bool expectSeparator{};
			while (cursor < stop) {
				const uint8_t current = state->charAt(cursor);
				if (current == internal::uint_c_v<','>) {
					++cursor;
					expectSeparator = false;
					continue;
				}
				if (current == internal::uint_c_v<'}'>) {
					state->cursor = cursor;
					skippedKeys	  = skipped;
					return value{ error_codes::no_such_field };
				}
				if (expectSeparator) [[unlikely]] {
					state->cursor = cursor;
					skippedKeys	  = skipped;
					return value{ error_codes::tape_error };
				}
				if (current == internal::uint_c_v<'"'> && cursor + 1 < tapeEnd && state->template charAt<internal::uint_c_v<':'>>(cursor + 1)) [[likely]] {
					internal::read_structural_index_ptr valueIter = cursor + 2;
					bool matched{};
					if constexpr (escapeAware) {
						const bool hasEscape = state->keyHasEscape(cursor);
						escapedKeySeen |= hasEscape;
						matched = hasEscape && state->unescapedKeyMatches(cursor, key);
					} else {
						matched = state->template keyMatches<minified>(cursor, key);
					}
					if (matched) {
						state->cursor = valueIter;
						skippedKeys	  = skipped;
						return value{ state, valueIter, depth };
					}
					++skipped;
					cursor = valueIter;
				}
				cursor			= state->skipValue(cursor);
				expectSeparator = true;
			}
			state->cursor = cursor;
			skippedKeys	  = skipped;
			return value{ cursor >= tapeEnd ? error_codes::unclosed_container : error_codes::no_such_field };
		}

		JSONIFIER_INLINE value trailHit(internal::read_structural_index_ptr keyIter, internal::read_structural_index_ptr resumePoint, bool resumed) const noexcept {
			if (resumed) {
				if (keyIter < resumePoint) {
					doc->noteBackwardHit();
				} else {
					doc->noteForwardHit();
				}
			}
			doc->cursor = keyIter + 2;
			return value{ doc, keyIter + 2, depth };
		}

		JSONIFIER_INLINE value trailLookup(string_view key, internal::read_structural_index_ptr resumePoint, bool resumed) noexcept {
			document_state* const state = doc;
			for (uint32_t x = 0; x < trailCount; ++x) {
				internal::read_structural_index_ptr keyIter = state->tape + trailEntry(x);
				if (state->template keyMatches<minified>(keyIter, key)) {
					return trailHit(keyIter, resumePoint, resumed);
				}
			}
			if (!trailComplete) {
				value found{ extendTrail(key, resumePoint, resumed) };
				if (found.error_code == error_codes::success || trailOverflow) {
					return found;
				}
			}
			if (!escapeFree) {
				bool sawEscape{};
				for (uint32_t x = 0; x < trailCount; ++x) {
					internal::read_structural_index_ptr keyIter = state->tape + trailEntry(x);
					if (state->keyHasEscape(keyIter)) {
						sawEscape = true;
						if (state->unescapedKeyMatches(keyIter, key)) {
							return trailHit(keyIter, resumePoint, resumed);
						}
					}
				}
				escapeFree = !sawEscape;
			}
			state->cursor = resumePoint;
			return value{ error_codes::no_such_field };
		}

		JSONIFIER_INLINE value indexedField(string_view key) noexcept {
			document_state* const state						= doc;
			internal::read_structural_index_ptr resumePoint = state->cursor;
			value found{ error_codes::no_such_field };
			if (!indexed) {
				if (!indexEarned) {
					const bool resumed = resumePoint != open + 1;
					if (!trailOverflow) {
						if (trailCount == 0 && !trailComplete && !(resumed && state->reverseMode())) {
							found = scanFields<false>(key, state->tapeEnd);
							if (found.error_code != error_codes::no_such_field) {
								if (found.error_code == error_codes::success && resumed) {
									state->noteForwardHit();
								}
								return found;
							}
						}
						found = trailLookup(key, resumePoint, resumed);
						if (!trailOverflow) {
							return found;
						}
						state->cursor = resumePoint;
					}
					if (resumed && state->reverseMode()) {
						found = scanBeforeResume(key, resumePoint);
						if (found.error_code == error_codes::no_such_field) {
							state->cursor = resumePoint;
							found		  = scanFields<false>(key, state->tapeEnd);
							if (found.error_code == error_codes::success) {
								state->noteForwardHit();
							}
						}
					} else {
						found = scanFields<false>(key, state->tapeEnd);
						if (found.error_code == error_codes::success) {
							if (resumed) {
								state->noteForwardHit();
							}
						} else if (found.error_code == error_codes::no_such_field && resumed) {
							found = scanBeforeResume(key, resumePoint);
						}
					}
					if (found.error_code == error_codes::no_such_field) {
						found = escapeAwareRetry(key, resumePoint);
					}
					return found;
				}
				indexAllFields();
				indexed = true;
			}

			const uint32_t object = objectIndex();
			const uint64_t hash	  = internal::field_index_map::hashKey(std::bit_cast<internal::read_buffer_ptr>(key.data()), key.size(), object);
			const uint32_t hit	  = state->fieldIndex->template find<minified, internal::key_matcher>(hash, object, state, key);
			if (hit != internal::field_index_map::npos) {
				state->cursor = state->tape + hit + 2;
				found		  = value{ state, state->cursor, depth };
			}
			if (found.error_code == error_codes::no_such_field) {
				found = escapeAwareRetry(key, resumePoint);
			}
			return found;
		}

		JSONIFIER_INLINE value escapeAwareRetry(string_view key, internal::read_structural_index_ptr restorePoint) noexcept {
			if (escapeFree) {
				doc->cursor = restorePoint;
				return value{ error_codes::no_such_field };
			}
			doc->cursor	   = open + 1;
			escapedKeySeen = false;
			value found{ scanFields<true>(key, doc->tapeEnd) };
			if (found.error_code == error_codes::no_such_field) {
				escapeFree	= !escapedKeySeen;
				doc->cursor = restorePoint;
			}
			return found;
		}

		JSONIFIER_INLINE value scanBeforeResume(string_view key, internal::read_structural_index_ptr resumePoint) noexcept {
			doc->cursor = open + 1;
			value found{ scanFields<false>(key, resumePoint) };
			indexEarned = ++wrapCount >= wrapThreshold && skippedKeys >= indexThreshold;
			if (found.error_code == error_codes::success) {
				doc->noteBackwardHit();
			}
			return found;
		}

		JSONIFIER_INLINE uint32_t trailEntry(uint32_t index) const noexcept {
			uint32_t keyOffset;
			internal::pow2MemcpyWrapper<sizeof(uint32_t)>(&keyOffset, trailBytes + index * sizeof(uint32_t));
			return keyOffset;
		}

		JSONIFIER_INLINE uint32_t objectIndex() const noexcept {
			return static_cast<uint32_t>(open - doc->tape);
		}

		JSONIFIER_INLINE void settle() const noexcept {
			if (doc->depth != depth) [[unlikely]] {
				if (doc->depth > depth) {
					doc->finishToDepth(depth);
				}
				if (doc->depth != depth) {
					rewind();
				}
			}
		}

		JSONIFIER_INLINE void rewind() const noexcept {
			doc->cursor = open + 1;
			doc->depth	= depth;
		}

		internal::read_structural_index_ptr open{};
		document_state* doc{};
		uint64_t depth{};
		uint32_t skippedKeys{};
		uint32_t trailCount{};
		uint32_t wrapCount{};
		uint8_t trailBytes[trailCapacity * sizeof(uint32_t)];
		error_codes error_code{ error_codes::success };
		bool escapedKeySeen{};
		bool trailComplete{};
		bool trailOverflow{};
		bool indexEarned{};
		bool escapeFree{};
		bool indexed{};
	};

}
