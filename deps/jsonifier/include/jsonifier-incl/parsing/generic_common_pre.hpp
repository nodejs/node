/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/parsing/generic_common_pre.hpp
 */
#pragma once

#include <jsonifier-incl/utilities/string.hpp>
#include <jsonifier-incl/utilities/string_view.hpp>
#include <jsonifier-incl/utilities/error.hpp>
#include <jsonifier-incl/utilities/str_to_i.hpp>
#include <jsonifier-incl/utilities/string_utils.hpp>
#include <jsonifier-incl/utilities/simd.hpp>
#include <jsonifier-incl/utilities/byteswap.hpp>

namespace jsonifier::internal {

	consteval array<bool, 256> genValidPostPrimitiveTable() {
		array<bool, 256> table{};
		table[static_cast<uint64_t>(',')]  = true;
		table[static_cast<uint64_t>('}')]  = true;
		table[static_cast<uint64_t>(']')]  = true;
		table[static_cast<uint64_t>(' ')]  = true;
		table[static_cast<uint64_t>('\t')] = true;
		table[static_cast<uint64_t>('\n')] = true;
		table[static_cast<uint64_t>('\r')] = true;
		return table;
	}

	alignas(64) inline constexpr auto validPostPrimitiveTable{ genValidPostPrimitiveTable() };

}

namespace jsonifier::generic {

	enum class error_codes : uint8_t {
		success,
		empty,
		tape_error,
		unclosed_container,
		trailing_content,
		incorrect_type,
		no_such_field,
		index_out_of_bounds,
		number_error,
		string_error,
		t_atom_error,
		f_atom_error,
		n_atom_error,
		invalid_json_pointer,
		capacity,
		count,
	};

	template<bool minified> class generic_object;
	template<bool minified> class generic_array;
	template<bool minified> class generic_field;

}

namespace jsonifier::internal {

	template<typename value_type> class alloc_buffer : internal::alloc_wrapper<value_type> {
	  public:
		using allocator = internal::alloc_wrapper<value_type>;

		JSONIFIER_INLINE alloc_buffer() noexcept = default;

		JSONIFIER_INLINE explicit alloc_buffer(uint64_t capacityNew) noexcept {
			reserve(capacityNew);
		}

		JSONIFIER_INLINE alloc_buffer& operator=(alloc_buffer&& other) noexcept {
			if (this != &other) [[likely]] {
				release();
				ptr		 = std::exchange(other.ptr, nullptr);
				capacity = std::exchange(other.capacity, uint64_t{});
			}
			return *this;
		}

		JSONIFIER_INLINE alloc_buffer(alloc_buffer&& other) noexcept : capacity{ std::exchange(other.capacity, uint64_t{}) }, ptr{ std::exchange(other.ptr, nullptr) } {
		}

		alloc_buffer& operator=(const alloc_buffer&) = delete;
		alloc_buffer(const alloc_buffer&)			 = delete;

		JSONIFIER_INLINE value_type* data() const noexcept {
			return ptr;
		}

		inline void reserve(uint64_t capacityNew) noexcept {
			if (capacityNew > capacity) {
				release();
				ptr		 = allocator::allocate(capacityNew);
				capacity = capacityNew;
			}
		}

		JSONIFIER_INLINE uint64_t size() const noexcept {
			return capacity;
		}

		JSONIFIER_INLINE ~alloc_buffer() noexcept {
			release();
		}

	  protected:
		JSONIFIER_INLINE void release() noexcept {
			if (ptr) {
				allocator::deallocate(ptr, capacity);
				ptr		 = nullptr;
				capacity = 0;
			}
		}

		uint64_t capacity{};
		value_type* ptr{};
	};

	struct string_arena {
		inline string_arena() noexcept = default;

		JSONIFIER_INLINE string_arena& operator=(string_arena&& other) noexcept {
			if (this != &other) [[likely]] {
				buffer	   = internal::move(other.buffer);
				scratchPtr = std::exchange(other.scratchPtr, nullptr);
				slots	   = std::exchange(other.slots, nullptr);
			}
			return *this;
		}

		JSONIFIER_INLINE string_arena(string_arena&& other) noexcept
			: buffer{ internal::move(other.buffer) }, scratchPtr{ std::exchange(other.scratchPtr, nullptr) }, slots{ std::exchange(other.slots, nullptr) } {
		}

		string_arena& operator=(const string_arena&) = delete;
		string_arena(const string_arena&)			 = delete;

		JSONIFIER_INLINE void reset(uint64_t documentLength) noexcept {
			buffer.reserve(documentLength * 2 + 128);
			slots	   = buffer.data();
			scratchPtr = buffer.data() + documentLength + 64;
		}

		JSONIFIER_INLINE char* slot(uint64_t offset) const noexcept {
			return slots + offset;
		}

		JSONIFIER_INLINE char* scratch() const noexcept {
			return scratchPtr;
		}

	  protected:
		alloc_buffer<char> buffer{};
		char* scratchPtr{};
		char* slots{};
	};

	struct utf8_span_step {
		template<uint64_t index>
		JSONIFIER_INLINE static unrestrict_t<read_buffer_ptr> impl(unrestrict_t<read_buffer_ptr>& current, read_buffer_ptr end, internal::utf8_validation_state& state) noexcept {
			using simd_list_local					 = type_list_element_t<index, simd::avx_integer_list>;
			static constexpr auto avx_type			 = simd_list_local::type::avx_type;
			using simd_type_local					 = typename simd_type_wrapper<avx_type>::type;
			static constexpr uint64_t bytesProcessed = simd_list_local::bytesProcessed;
			internal::utf8_register_validator<simd_type_wrapper<avx_type>> validator{ state };
			while (static_cast<uint64_t>(end - current) >= bytesProcessed) {
				validator.checkRegister(simd::gatherValuesU<simd_type_local>(current));
				current += bytesProcessed;
			}
			if constexpr (index == listSize - 1) {
				validator.checkPartial(current, static_cast<uint64_t>(end - current));
				current = end;
				return validator.errors() ? nullptr : current;
			} else {
				validator.flush();
				return current;
			}
		}
	};

	struct key_matcher {
		template<bool minified, typename state_type, typename key_type> JSONIFIER_INLINE static bool impl(state_type&& state, key_type&& key, uint32_t keyIndex) noexcept {
			return state->template keyMatches<minified>(state->tape + keyIndex, key);
		}
	};

	struct document_state;
	class field_index_map;

	struct plain_string_copy_context {
		plain_string_copy_context() noexcept = delete;

		JSONIFIER_INLINE plain_string_copy_context(read_buffer_ptr sourceNew) noexcept : source{ sourceNew } {
		}

		plain_string_copy_context& operator=(plain_string_copy_context&&) noexcept		= delete;
		plain_string_copy_context(plain_string_copy_context&&) noexcept					= delete;
		plain_string_copy_context& operator=(const plain_string_copy_context&) noexcept = delete;
		plain_string_copy_context(const plain_string_copy_context&) noexcept			= delete;

		JSONIFIER_INLINE uint64_t operator()(char* __restrict ptrNew, uint64_t length) const noexcept {
			jsonifierMemcpy(ptrNew, source, length);
			return length;
		}

		read_buffer_ptr source{};
	};

}
