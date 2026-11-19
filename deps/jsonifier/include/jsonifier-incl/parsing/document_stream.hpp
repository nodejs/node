/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/parsing/document_stream.hpp
 */
#pragma once

#include <jsonifier-incl/parsing/generic_document.hpp>
#include <jsonifier-incl/parsing/generic_common_post.hpp>

namespace jsonifier::generic {

	template<typename parser_type, parse_options options> class document_stream {
	  public:
		using document_state = internal::document_state;
		using document		 = generic_document<options.minified>;

		class iterator {
		  public:
			using iterator_category = std::input_iterator_tag;
			using value_type		= document;
			using difference_type	= std::ptrdiff_t;

			JSONIFIER_INLINE iterator() noexcept = default;

			JSONIFIER_INLINE explicit iterator(document_stream* streamNew) noexcept : stream{ streamNew } {
			}

			JSONIFIER_INLINE bool operator==(const iterator& other) const noexcept {
				return exhausted() == other.exhausted();
			}

			JSONIFIER_INLINE uint64_t currentIndex() const noexcept {
				return stream->currentIndex();
			}

			JSONIFIER_INLINE string_view source() const noexcept {
				return stream->source();
			}

			JSONIFIER_INLINE document operator*() const noexcept {
				return stream->current();
			}

			JSONIFIER_INLINE void operator++(int32_t) noexcept {
				stream->advance();
			}

			JSONIFIER_INLINE iterator& operator++() noexcept {
				stream->advance();
				return *this;
			}

		  protected:
			JSONIFIER_INLINE bool exhausted() const noexcept {
				return !stream || stream->finished;
			}

			document_stream* stream{};
		};

		JSONIFIER_INLINE document_stream(parser_type& parserNew, internal::read_buffer_ptr inputNew, uint64_t inputLengthNew, uint64_t batchSizeNew) noexcept
			: input{ inputNew }, inputLength{ inputNew ? inputLengthNew : 0 }, parser{ &parserNew },
			  batchSize{ std::clamp<uint64_t>(batchSizeNew, minimumBatchSize, maximumBatchSize) } {
		}

		document_stream& operator=(const document_stream&) = delete;
		document_stream(const document_stream&)			   = delete;

		JSONIFIER_INLINE uint64_t truncatedBytes() const noexcept {
			return truncated;
		}

		JSONIFIER_INLINE error_codes error() const noexcept {
			return error_code;
		}

		JSONIFIER_INLINE iterator end() const noexcept {
			return iterator{};
		}

		JSONIFIER_INLINE iterator begin() noexcept {
			if (!started) {
				started = true;
				if (inputLength == 0) {
					finished = true;
				} else {
					loadWindow(0);
					locate(windowTape);
				}
			}
			return iterator{ this };
		}

	  protected:
		static constexpr uint64_t minimumBatchSize{ 64 };
		static constexpr uint64_t maximumBatchSize{ std::numeric_limits<uint32_t>::max() - 64 };

		JSONIFIER_INLINE internal::unrestrict_t<internal::read_structural_index_ptr> findDocumentEnd(internal::read_structural_index_ptr start) noexcept {
			document_state& state = docState();
			const uint8_t first	  = state.charAt(start);
			if (document_state::isOpener(first)) {
				int64_t openCount{ 1 };
				for (internal::read_structural_index_ptr iter = start + 1; iter < windowTapeEnd; ++iter) {
					openCount += internal::depthDeltaTable[state.charAt(iter)];
					if (openCount == 0) {
						return iter + 1;
					}
				}
				return nullptr;
			}
			if (document_state::isCloser(first) || first == internal::uint_c_v<','> || first == internal::uint_c_v<':'>) [[unlikely]] {
				error_code = error_codes::tape_error;
				return nullptr;
			}
			if (start + 1 == windowTapeEnd && !finalWindow) {
				return nullptr;
			}
			return start + 1;
		}

		JSONIFIER_INLINE void bindDocument(internal::read_structural_index_ptr begin, internal::read_structural_index_ptr end) noexcept {
			docBegin			  = begin;
			docEnd				  = end;
			pendingSeparator	  = false;
			document_state& state = docState();
			state.tape			  = begin;
			state.tapeEnd		  = end;
			state.cursor		  = begin;
			state.depth			  = 0;
			storage().fieldIndex.reset();
		}

		JSONIFIER_INLINE internal::unrestrict_t<internal::read_structural_index_ptr> lastDocumentStart() const noexcept {
			document_state& state = docState();
			for (internal::read_structural_index_ptr iter = windowTapeEnd - 1; iter > windowTape; --iter) {
				if (startsDocument(state.charAt(iter)) && endsValue(state.charAt(iter - 1))) {
					return iter;
				}
			}
			return windowTape;
		}

		JSONIFIER_INLINE static bool startsDocument(uint8_t c) noexcept {
			return !document_state::isCloser(c) && c != internal::uint_c_v<','> && c != internal::uint_c_v<':'>;
		}

		JSONIFIER_INLINE static bool endsValue(uint8_t c) noexcept {
			return !document_state::isOpener(c) && c != internal::uint_c_v<','> && c != internal::uint_c_v<':'>;
		}

		JSONIFIER_INLINE void locateDelimited(internal::read_structural_index_ptr next) noexcept {
			while (true) {
				if (next == windowTapeEnd) {
					if (finalWindow) {
						if (pendingSeparator) [[unlikely]] {
							error_code = error_codes::trailing_content;
						} else {
							finished = true;
						}
						return;
					}
					loadWindow(windowStart + windowLength);
					next = windowTape;
					continue;
				}
				if (docBegin && !pendingSeparator && docState().template charAt<internal::uint_c_v<','>>(next)) {
					++next;
					pendingSeparator = true;
					continue;
				}
				if (internal::read_structural_index_ptr documentEnd = findDocumentEnd(next); documentEnd) [[likely]] {
					bindDocument(next, documentEnd);
					return;
				}
				if (error_code != error_codes::success) [[unlikely]] {
					return;
				}
				if (finalWindow) {
					truncated = windowLength - *next;
					finished  = true;
					return;
				}
				if (next == windowTape) [[unlikely]] {
					error_code = error_codes::capacity;
					return;
				}
				loadWindow(windowStart + *next);
				next = windowTape;
			}
		}

		JSONIFIER_INLINE internal::unrestrict_t<internal::read_structural_index_ptr> consumedEnd() const noexcept {
			document_state& state = docState();
			if (state.depth > 0) {
				state.finishToDepth(0);
				return state.cursor;
			}
			if (state.cursor > docBegin) {
				return state.cursor;
			}
			return state.skipValue(docBegin);
		}

		JSONIFIER_INLINE void locateWhitespace(internal::read_structural_index_ptr next) noexcept {
			while (true) {
				if (next == completeEnd) {
					if (finalWindow) {
						finished = true;
						return;
					}
					if (completeEnd == windowTape) [[unlikely]] {
						error_code = error_codes::capacity;
						return;
					}
					loadWindow(windowStart + (completeEnd == windowTapeEnd ? windowLength : *completeEnd));
					next = windowTape;
					continue;
				}
				if (!startsDocument(docState().charAt(next))) [[unlikely]] {
					error_code = error_codes::tape_error;
					return;
				}
				bindDocument(next, completeEnd);
				return;
			}
		}

		JSONIFIER_INLINE void findCompleteEnd() noexcept {
			if (windowTape == windowTapeEnd) {
				completeEnd = windowTapeEnd;
				return;
			}
			internal::read_structural_index_ptr lastStart = lastDocumentStart();
			if (lastStart != windowTape && !finalWindow) {
				completeEnd = lastStart;
				return;
			}
			if (!startsDocument(docState().charAt(lastStart)) || findDocumentEnd(lastStart)) {
				completeEnd = windowTapeEnd;
				return;
			}
			completeEnd = lastStart;
			if (finalWindow) {
				truncated = windowLength - *lastStart;
			}
		}

		JSONIFIER_INLINE string_view source() const noexcept {
			if (error_code != error_codes::success) [[unlikely]] {
				return {};
			}
			internal::read_buffer_ptr base = input + windowStart;
			const uint64_t start		   = *docBegin;
			uint64_t stop{};
			if constexpr (!options.newLineDelimited) {
				stop = *docEnd;
			} else {
				stop = *docState().skipValue(docBegin);
			}
			while (stop > start && isSeparatorByte(base[stop - 1])) {
				--stop;
			}
			return string_view{ std::bit_cast<const char*>(base + start), stop - start };
		}

		JSONIFIER_INLINE uint64_t currentIndex() const noexcept {
			return error_code == error_codes::success ? windowStart + *docBegin : windowStart;
		}

		JSONIFIER_INLINE void locate(internal::read_structural_index_ptr next) noexcept {
			if constexpr (!options.newLineDelimited) {
				locateDelimited(next);
			} else {
				locateWhitespace(next);
			}
		}

		JSONIFIER_INLINE void loadWindow(uint64_t start) noexcept {
			windowStart							 = start;
			windowLength						 = std::min(batchSize, inputLength - start);
			finalWindow							 = start + windowLength == inputLength;
			internal::read_buffer_ptr windowRoot = input + start;
			parser->template indexGenericWindow<options>(windowRoot, windowLength);
			internal::generic_storage& data = storage();
			document_state& state			= data.stateFor();
			windowTape						= state.tape;
			windowTapeEnd					= state.tapeEnd;
			state.root						= windowRoot;
			state.end						= windowRoot + windowLength;
			data.arena.reset(windowLength);
			state.arena		 = &data.arena;
			state.fieldIndex = &data.fieldIndex;
			if constexpr (options.newLineDelimited) {
				findCompleteEnd();
			}
		}

		JSONIFIER_INLINE static bool isSeparatorByte(uint8_t c) noexcept {
			return internal::seperatorByteTable[c];
		}

		JSONIFIER_INLINE internal::generic_storage& storage() const noexcept {
			return parser->genericStorage();
		}

		JSONIFIER_INLINE document_state& docState() const noexcept {
			return storage().stateFor();
		}

		JSONIFIER_INLINE document current() const noexcept {
			if (error_code != error_codes::success) [[unlikely]] {
				return document{ error_code };
			}
			return document{ &docState(), docBegin, 0 };
		}

		JSONIFIER_INLINE void advance() noexcept {
			if (error_code != error_codes::success) [[unlikely]] {
				finished = true;
				return;
			}
			if constexpr (!options.newLineDelimited) {
				locate(docEnd);
			} else {
				locate(consumedEnd());
			}
		}

		internal::read_structural_index_ptr windowTapeEnd{};
		internal::read_structural_index_ptr completeEnd{};
		internal::read_structural_index_ptr windowTape{};
		internal::read_structural_index_ptr docBegin{};
		internal::read_structural_index_ptr docEnd{};
		internal::read_buffer_ptr input{};
		uint64_t windowLength{};
		uint64_t inputLength{};
		uint64_t windowStart{};
		parser_type* parser{};
		uint64_t batchSize{};
		uint64_t truncated{};
		error_codes error_code{ error_codes::success };
		bool pendingSeparator{};
		bool finalWindow{};
		bool finished{};
		bool started{};
	};

}
