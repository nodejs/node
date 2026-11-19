/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/containers/allocator.hpp
 */
#pragma once

#include <jsonifier-incl/simd/simd_config.hpp>
#include <jsonifier-incl/utilities/utility.hpp>

namespace jsonifier::internal {

	template<auto multiple, typename value_type = decltype(multiple)> JSONIFIER_INLINE constexpr value_type roundDownToMultiple(value_type value) noexcept {
		if constexpr ((multiple & (multiple - 1)) == 0) {
			constexpr auto notMulSub1{ ~(multiple - 1) };
			return value & notMulSub1;
		} else {
			if constexpr (std::is_signed_v<value_type>) {
				return value >= 0 ? (value / multiple) * multiple : ((value - multiple + 1) / multiple) * multiple;
			} else {
				return (value / multiple) * multiple;
			}
		}
	}

	template<auto multiple, typename value_type = decltype(multiple)> JSONIFIER_INLINE constexpr value_type roundUpToMultiple(value_type value) noexcept {
		if constexpr ((multiple & (multiple - 1)) == 0) {
			constexpr auto mulSub1{ multiple - 1 };
			constexpr auto notMulSub1{ ~mulSub1 };
			return (value + static_cast<value_type>(mulSub1)) & static_cast<value_type>(notMulSub1);
		} else {
			const auto remainder = value % multiple;
			return remainder == 0 ? value : value + (multiple - remainder);
		}
	}

	enum class allocated_memory_types : uint8_t {
		huge_page,
		mmap,
		standard_page,
		standard,
	};

	template<trivially_constructible_copyable_destructible_types value_type_new> struct alloc_wrapper {
		using value_type					= value_type_new;
		using size_type						= uint64_t;
		using difference_type				= ptrdiff_t;
		using pointer						= value_type*;
		using const_pointer					= const value_type*;
		static constexpr uint64_t alignment = (alignof(value_type_new) > simdBytesPerRegister) ? alignof(value_type_new) : simdBytesPerRegister;

		struct allocation_header {
			size_type totalBytes{};
			allocated_memory_types allocated_memory_type{};
		};

		static constexpr uint64_t headerSize		= roundUpToMultiple<alignment>(sizeof(allocation_header));
		static constexpr uint64_t hugePageSize		= 2 * 1024 * 1024ULL;
		static constexpr uint64_t hugePageThreshold = hugePageSize * 2;

		using allocator_traits = std::allocator_traits<alloc_wrapper<value_type>>;

		template<typename other_type> struct rebind {
			using other = alloc_wrapper<other_type>;
		};

		JSONIFIER_INLINE constexpr alloc_wrapper() noexcept = default;

		template<typename other_type> JSONIFIER_INLINE constexpr alloc_wrapper(const alloc_wrapper<other_type>&) noexcept {
		}

		JSONIFIER_INLINE static pointer finalize(void* base, allocated_memory_types allocated_memory_type, size_type totalBytes) noexcept {
			allocation_header* header	  = static_cast<allocation_header*>(base);
			header->totalBytes			  = totalBytes;
			header->allocated_memory_type = allocated_memory_type;
			return std::bit_cast<pointer>(static_cast<std::byte*>(base) + headerSize);
		}

		template<typename... arg_types> JSONIFIER_INLINE static void construct(pointer p, arg_types&&... args) noexcept {
			new (p) value_type(internal::forward<arg_types>(args)...);
		}

		JSONIFIER_INLINE static pointer allocate(size_type count) noexcept {
			if (count == 0) [[unlikely]] {
				return nullptr;
			}
			if (count > maxSize()) [[unlikely]] {
				return nullptr;
			}
			const size_type bytes		 = count * sizeof(value_type) + headerSize;
			const size_type alignedBytes = roundUpToMultiple<alignment>(bytes);
			if (alignedBytes >= hugePageThreshold) {
				[[maybe_unused]] const size_type hpBytes = roundUpToMultiple<hugePageSize>(alignedBytes);
#if JSONIFIER_PLATFORM_WINDOWS
				void* p = VirtualAlloc(nullptr, hpBytes, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
				if (p != nullptr) {
					return finalize(p, allocated_memory_types::huge_page, hpBytes);
				}
				p = VirtualAlloc(nullptr, hpBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
				if (p != nullptr) {
					return finalize(p, allocated_memory_types::standard_page, hpBytes);
				}
#elif JSONIFIER_PLATFORM_LINUX
				void* p = mmap(nullptr, hpBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
				if (p != MAP_FAILED) {
					return finalize(p, allocated_memory_types::huge_page, hpBytes);
				}
				p = mmap(nullptr, hpBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
				if (p != MAP_FAILED) {
					madvise(p, hpBytes, MADV_HUGEPAGE);
					return finalize(p, allocated_memory_types::mmap, hpBytes);
				}
#elif JSONIFIER_PLATFORM_MAC || JSONIFIER_PLATFORM_ANDROID
				void* p = mmap(nullptr, hpBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
				if (p != MAP_FAILED) {
					return finalize(p, allocated_memory_types::mmap, hpBytes);
				}
#endif
			}
#if (JSONIFIER_PLATFORM_WINDOWS || JSONIFIER_PLATFORM_LINUX) && JSONIFIER_ANY_AVX
			void* p = _mm_malloc(alignedBytes, alignment);
#elif JSONIFIER_PLATFORM_WINDOWS
			void* p = _aligned_malloc(alignedBytes, alignment);
#else
			void* p = aligned_alloc(alignment, alignedBytes);
#endif
			if (p == nullptr) [[unlikely]] {
				return nullptr;
			}
			return finalize(p, allocated_memory_types::standard, alignedBytes);
		}

		static consteval size_type maxSize() noexcept {
			return (static_cast<size_type>(-1) - headerSize - (alignment - 1)) / sizeof(value_type);
		}

		JSONIFIER_INLINE constexpr bool operator!=(const alloc_wrapper& other) const noexcept {
			return !(*this == other);
		}

		JSONIFIER_INLINE static void deallocate(pointer p, size_type) noexcept {
			if (p) [[likely]] {
				void* base							  = std::bit_cast<std::byte*>(p) - headerSize;
				auto* header						  = static_cast<allocation_header*>(base);
				[[maybe_unused]] size_type totalBytes = header->totalBytes;
				switch (static_cast<uint64_t>(header->allocated_memory_type)) {
					case static_cast<uint64_t>(allocated_memory_types::huge_page): {
#if JSONIFIER_PLATFORM_WINDOWS
						VirtualFree(base, 0, MEM_RELEASE);
#elif JSONIFIER_PLATFORM_LINUX
						munmap(base, totalBytes);
#endif
						break;
					}
					case static_cast<uint64_t>(allocated_memory_types::mmap): {
#if JSONIFIER_PLATFORM_LINUX || JSONIFIER_PLATFORM_MAC || JSONIFIER_PLATFORM_ANDROID
						munmap(base, totalBytes);
#endif
						break;
					}
					case static_cast<uint64_t>(allocated_memory_types::standard_page): {
#if JSONIFIER_PLATFORM_WINDOWS
						VirtualFree(base, 0, MEM_RELEASE);
#elif JSONIFIER_PLATFORM_LINUX || JSONIFIER_PLATFORM_MAC
						munmap(base, totalBytes);
#endif
						break;
					}
					case static_cast<uint64_t>(allocated_memory_types::standard): {
#if (JSONIFIER_PLATFORM_WINDOWS || JSONIFIER_PLATFORM_LINUX) && JSONIFIER_ANY_AVX
						_mm_free(base);
#elif JSONIFIER_PLATFORM_WINDOWS
						_aligned_free(base);
#else
						free(base);
#endif
						break;
					}
					default: {
						break;
					}
				}
			}
		}

		JSONIFIER_INLINE constexpr bool operator==(const alloc_wrapper&) const noexcept {
			return true;
		}

		JSONIFIER_INLINE static constexpr void destroy(pointer) noexcept {
		}

		static_assert(alignment >= alignof(allocation_header), "alignment must cover allocation_header's alignment requirement.");
	};

}
