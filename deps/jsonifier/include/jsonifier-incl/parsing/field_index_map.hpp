/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/parsing/field_index_map.hpp
 */
#pragma once

#include <jsonifier-incl/parsing/generic_common_pre.hpp>

namespace jsonifier::internal {

	struct field_index_group {
#if JSONIFIER_NEON || JSONIFIER_SVE2
		static constexpr uint64_t laneShift{ 2 };
		static constexpr uint64_t laneMask{ 0x8888888888888888ull };
#else
		static constexpr uint64_t laneShift{ 0 };
		static constexpr uint64_t laneMask{ 0xFFFFFFFFFFFFFFFFull };
#endif

		JSONIFIER_INLINE static uint64_t match(const int8_t* controls, int8_t control) noexcept {
			const jsonifier_simd_int_128 group	 = simd::gatherValuesU<jsonifier_simd_int_128>(controls);
			const jsonifier_simd_int_128 needles = simd::gatherValue<jsonifier_simd_int_128>(static_cast<char>(control));
			return static_cast<uint64_t>(simd::opBitMaskRaw(simd::opCmpEqRaw(group, needles))) & laneMask;
		}

		JSONIFIER_INLINE static uint64_t firstLane(uint64_t mask) noexcept {
			return static_cast<uint64_t>(simd::countrZeroUnsafe(mask)) >> laneShift;
		}

		JSONIFIER_INLINE static uint64_t clearLane(uint64_t mask) noexcept {
			return simd::blsr(mask);
		}
	};

	class field_index_map {
	  public:
		static constexpr uint32_t npos{ std::numeric_limits<uint32_t>::max() };

		JSONIFIER_INLINE field_index_map() noexcept = default;

		JSONIFIER_INLINE field_index_map& operator=(field_index_map&& other) noexcept {
			if (this != &other) [[likely]] {
				controls  = internal::move(other.controls);
				entries	  = internal::move(other.entries);
				slotCount = std::exchange(other.slotCount, uint32_t{});
				groupMask = std::exchange(other.groupMask, uint64_t{});
				count	  = std::exchange(other.count, uint64_t{});
			}
			return *this;
		}

		JSONIFIER_INLINE field_index_map(field_index_map&& other) noexcept
			: controls{ internal::move(other.controls) }, entries{ internal::move(other.entries) }, groupMask{ std::exchange(other.groupMask, uint64_t{}) },
			  count{ std::exchange(other.count, uint64_t{}) }, slotCount{ std::exchange(other.slotCount, uint32_t{}) } {
		}

		field_index_map& operator=(const field_index_map&) = delete;
		field_index_map(const field_index_map&)			   = delete;

		JSONIFIER_INLINE static uint64_t hashKey(read_buffer_ptr keyData, uint64_t keyLength, uint32_t objectIndex) noexcept {
			uint64_t low{};
			uint64_t high{};
			if (keyLength > 16) {
				return mix64(longKeyHash(keyData, keyLength) ^ (static_cast<uint64_t>(objectIndex) << 32), mixMultiplier);
			}
			if (keyLength >= 8) {
				low	 = loadWord<uint64_t>(keyData);
				high = loadWord<uint64_t>(keyData + keyLength - 8);
			} else if (keyLength >= 4) {
				low	 = loadWord<uint32_t>(keyData);
				high = loadWord<uint32_t>(keyData + keyLength - 4);
			} else if (keyLength > 0) {
				low = static_cast<uint64_t>(static_cast<uint8_t>(keyData[0])) | (static_cast<uint64_t>(static_cast<uint8_t>(keyData[keyLength >> 1])) << 8) |
					(static_cast<uint64_t>(static_cast<uint8_t>(keyData[keyLength - 1])) << 16);
			}
			const uint64_t lengthAndObject = (static_cast<uint64_t>(objectIndex) << 32) | keyLength;
			return mix64(low ^ hashSeed ^ lengthAndObject, mixMultiplier) ^ mix64(high + mixMultiplier, hashSeed ^ lengthAndObject);
		}

		template<bool minified, typename matcher_type, typename state_type, typename key_type>
		JSONIFIER_INLINE uint32_t find(uint64_t hash, uint32_t objectIndex, state_type&& state, key_type&& key) const noexcept {
			const int8_t control = static_cast<int8_t>(hash & 0x7F);
			uint64_t group		 = (hash >> 7) & groupMask;
			for (uint64_t probe = 0; probe <= groupMask; ++probe) {
				const int8_t* groupControls = controls.data() + group * groupWidth;
				for (uint64_t mask = field_index_group::match(groupControls, control); mask != 0;) {
					const uint64_t lane	   = field_index_group::firstLane(mask);
					const entry& candidate = entries.data()[group * groupWidth + lane];
					if (candidate.hash == hash && candidate.object == objectIndex && matcher_type::template impl<minified>(state, key, candidate.key)) {
						return candidate.key;
					}
					mask = field_index_group::clearLane(mask);
				}
				if (field_index_group::match(groupControls, emptyControl) != 0) {
					return npos;
				}
				group = (group + 1) & groupMask;
			}
			return npos;
		}

		JSONIFIER_INLINE void insert(uint64_t hash, uint32_t objectIndex, uint32_t keyIndex) noexcept {
			if ((count + 1) * 8 > uint64_t{ slotCount } * 7) [[unlikely]] {
				grow();
			}
			place(entry{ hash, objectIndex, keyIndex });
			++count;
		}

		JSONIFIER_INLINE void reset() noexcept {
			if (count != 0) {
				std::memset(controls.data(), static_cast<uint8_t>(emptyControl), slotCount);
				count = 0;
			}
		}

		JSONIFIER_INLINE uint64_t size() const noexcept {
			return count;
		}

	  protected:
		struct entry {
			uint64_t hash;
			uint32_t object;
			uint32_t key;
		};

		static constexpr uint64_t groupWidth{ 16 };
		static constexpr uint64_t initialSlots{ 128 };
		static constexpr int8_t emptyControl{ static_cast<int8_t>(-128) };
		static constexpr uint64_t hashSeed{ 14695981039346656037ull };
		static constexpr uint64_t mixMultiplier{ 0x9DDFEA08EB382D69ull };

		JSONIFIER_INLINE void place(const entry& newEntry) noexcept {
			uint64_t group = (newEntry.hash >> 7) & groupMask;
			while (true) {
				int8_t* groupControls = controls.data() + group * groupWidth;
				if (const uint64_t empties = field_index_group::match(groupControls, emptyControl); empties != 0) [[likely]] {
					const uint64_t lane						  = field_index_group::firstLane(empties);
					groupControls[lane]						  = static_cast<int8_t>(newEntry.hash & 0x7F);
					entries.data()[group * groupWidth + lane] = newEntry;
					return;
				}
				group = (group + 1) & groupMask;
			}
		}

		template<typename word_type> JSONIFIER_INLINE static uint64_t loadWord(read_buffer_ptr source) noexcept {
			word_type word;
			pow2MemcpyWrapper<sizeof(word_type)>(&word, source);
			return static_cast<uint64_t>(word);
		}

		JSONIFIER_INLINE static uint64_t longKeyHash(read_buffer_ptr keyData, uint64_t keyLength) noexcept {
			static constexpr uint64_t c1{ 0x87c37b91114253d5ull };
			static constexpr uint64_t c2{ 0x4cf5ad432745937full };
			uint64_t h1			   = hashSeed;
			uint64_t h2			   = hashSeed;
			const uint64_t nblocks = keyLength / 16;
			for (uint64_t block = 0; block < nblocks; ++block) {
				uint64_t k1 = loadWord<uint64_t>(keyData + block * 16);
				uint64_t k2 = loadWord<uint64_t>(keyData + block * 16 + 8);
				k1 *= c1;
				k1 = rotl64(k1, 31);
				k1 *= c2;
				h1 ^= k1;
				h1 = rotl64(h1, 27);
				h1 += h2;
				h1 = h1 * 5 + 0x52dce729;
				k2 *= c2;
				k2 = rotl64(k2, 33);
				k2 *= c1;
				h2 ^= k2;
				h2 = rotl64(h2, 31);
				h2 += h1;
				h2 = h2 * 5 + 0x38495ab5;
			}
			const uint64_t tailLength = keyLength & 15;
			if (tailLength != 0) {
				read_buffer_ptr tail = keyData + keyLength - 16;
				uint64_t k1			 = loadWord<uint64_t>(tail);
				uint64_t k2			 = loadWord<uint64_t>(tail + 8);
				k1 *= c1;
				k1 = rotl64(k1, 31);
				k1 *= c2;
				h1 ^= k1;
				k2 *= c2;
				k2 = rotl64(k2, 33);
				k2 *= c1;
				h2 ^= k2;
			}
			h1 ^= keyLength;
			h2 ^= keyLength;
			h1 += h2;
			h2 += h1;
			h1 = fmix64(h1);
			h2 = fmix64(h2);
			return h1 + h2;
		}

		JSONIFIER_INLINE static uint64_t mix64(uint64_t value, uint64_t multiplier) noexcept {
#if JSONIFIER_HAS_INT128
			const __uint128_t product = static_cast<__uint128_t>(value) * static_cast<__uint128_t>(multiplier);
			return static_cast<uint64_t>(product) ^ static_cast<uint64_t>(product >> 64);
#elif JSONIFIER_COMPILER_MSVC && JSONIFIER_ARCH_X64
			uint64_t highPart;
			const uint64_t lowPart = _umul128(value, multiplier, &highPart);
			return lowPart ^ highPart;
#else
			const uint64_t lowPart = value * multiplier;
			return lowPart ^ ((value >> 32) * (multiplier >> 32));
#endif
		}

		inline void grow() noexcept {
			const uint64_t newSlotCount = slotCount == 0 ? initialSlots : uint64_t{ slotCount } * 2;
			alloc_buffer<int8_t> oldControls{ internal::move(controls) };
			alloc_buffer<entry> oldEntries{ internal::move(entries) };
			const uint64_t oldSlotCount = slotCount;
			controls.reserve(newSlotCount);
			entries.reserve(newSlotCount);
			std::memset(controls.data(), static_cast<uint8_t>(emptyControl), newSlotCount);
			slotCount = static_cast<uint32_t>(newSlotCount);
			groupMask = newSlotCount / groupWidth - 1;
			for (uint64_t slot = 0; slot < oldSlotCount; ++slot) {
				if (oldControls.data()[slot] >= 0) {
					place(oldEntries.data()[slot]);
				}
			}
		}

		JSONIFIER_INLINE static uint64_t rotl64(uint64_t value, int32_t shift) noexcept {
			return (value << shift) | (value >> (64 - shift));
		}

		JSONIFIER_INLINE static uint64_t fmix64(uint64_t value) noexcept {
			value ^= value >> 33;
			value *= 0xff51afd7ed558ccdull;
			value ^= value >> 33;
			value *= 0xc4ceb9fe1a85ec53ull;
			value ^= value >> 33;
			return value;
		}

		alloc_buffer<int8_t> controls{};
		alloc_buffer<entry> entries{};
		uint64_t groupMask{};
		uint64_t count{};
		uint32_t slotCount{};
	};

}
