/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/utilities/simd.hpp
 */
// Sampled from Dr. Lemire's library, simdjson: https://github.com/simdjson/simdjson
#pragma once

#include <jsonifier-incl/utilities/string_view.hpp>
#include <jsonifier-incl/utilities/utility.hpp>
#include <jsonifier-incl/simd/add_tape_values.hpp>
#include <jsonifier-incl/simd/avx_stage1.hpp>
#include <jsonifier-incl/simd/neon_stage1.hpp>
#include <jsonifier-incl/core/fastio.hpp>

namespace jsonifier::internal {

	inline void printBitsAligned(uint64_t bits, const char* label, read_buffer_ptr str = nullptr, uint64_t len = 0) noexcept {
		cout << label << ":" << endl;
		if (str && len > 0) {
			cout << "STR:  ";
			for (uint64_t i = 0; i < std::min<uint64_t>(len, 64); ++i) {
				char c = static_cast<char>(str[i]);
				if (c == '\n' || c == '\r' || c == '\t') {
					c = ' ';
				}
				cout << c;
			}
			cout << endl;
		}
		cout << "BITS: ";
		for (uint64_t i = 0; i < 64; ++i) {
			cout << ((bits >> i) & 1ULL);
		}
		cout << endl;
		cout << "IDX:  ";
		for (uint64_t i = 0; i < 64; ++i) {
			cout << (i % 10);
		}
		cout << endl;
		cout << "TENS: ";
		for (uint64_t i = 0; i < 64; ++i) {
			cout << ((i / 10) % 10);
		}
		cout << endl << endl;
	}

	struct tail_block_builder {
		static constexpr uint64_t pageSize = 4096;

		alignas(64) static constexpr uint8_t laneIndices[64]{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
			32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63 };

		JSONIFIER_INLINE static void buildInPlace(write_buffer_ptr dst, read_buffer_ptr src, uint64_t remaining, uint64_t paddedBytes) noexcept {
			const jsonifier_simd_int_t spaces = simd::gatherValue<jsonifier_simd_int_t>(static_cast<char>(0x20));
			const jsonifier_simd_int_t lanes  = simd::gatherValues<jsonifier_simd_int_t>(+laneIndices);
			for (uint64_t offset = 0; offset < paddedBytes; offset += simdBytesPerRegister) {
				const uint64_t lanesLeft		= remaining > offset ? std::min<uint64_t>(remaining - offset, simdBytesPerRegister) : 0;
				const jsonifier_simd_int_t keep = simd::opCmpLtRaw(lanes, simd::gatherValue<jsonifier_simd_int_t>(static_cast<char>(lanesLeft)));
				const jsonifier_simd_int_t data = simd::gatherValuesU<jsonifier_simd_int_t>(src + offset);
				simd::storeU(simd::opXor(spaces, simd::opAnd(simd::opXor(data, spaces), keep)), dst + offset);
			}
		}

		JSONIFIER_INLINE static void buildByCopy(write_buffer_ptr dst, read_buffer_ptr src, uint64_t remaining, uint64_t paddedBytes) noexcept {
			std::memset(dst + remaining, static_cast<uint8_t>(0x20), paddedBytes - remaining);
			jsonifierMemcpy(dst, src, remaining);
		}

		JSONIFIER_INLINE static void impl(write_buffer_ptr dst, read_buffer_ptr src, uint64_t remaining, uint64_t paddedBytes) noexcept {
			if (readStaysInPage(src, remaining, paddedBytes)) [[likely]] {
				buildInPlace(dst, src, remaining, paddedBytes);
			} else {
				buildByCopy(dst, src, remaining, paddedBytes);
			}
		}

		JSONIFIER_INLINE static bool readStaysInPage(read_buffer_ptr src, uint64_t remaining, uint64_t paddedBytes) noexcept {
			if constexpr (JSONIFIER_ASAN_ENABLED) {
				return false;
			} else {
				const uint64_t lastValid = static_cast<uint64_t>(std::bit_cast<uintptr_t>(src)) + remaining - 1;
				const uint64_t lastRead	 = static_cast<uint64_t>(std::bit_cast<uintptr_t>(src)) + paddedBytes - 1;
				return (lastValid ^ lastRead) < pageSize;
			}
		}
	};

	struct string_block_reader {
		static constexpr uint64_t stepBytes = simdBlocksPerStep * 64;

		JSONIFIER_INLINE unrestrict_t<read_buffer_ptr> getRemainder(uint64_t paddedBytes) noexcept {
			tail_block_builder::impl(+block, inString + index, length - index, paddedBytes);
			return +block;
		}

		JSONIFIER_INLINE void reset(read_buffer_ptr stringViewNew, uint64_t lengthNew) noexcept {
			lengthMinusStep = lengthNew < stepBytes ? 0 : lengthNew - stepBytes;
			inString		= std::bit_cast<read_buffer_ptr>(stringViewNew);
			length			= lengthNew;
			index			= 0;
		}

		JSONIFIER_INLINE unrestrict_t<read_buffer_ptr> fullBlock() noexcept {
			read_buffer_ptr newPtr = inString + index;
			index += stepBytes;
			return newPtr;
		}

		JSONIFIER_INLINE uint64_t getRemainderBytes() const noexcept {
			return length - index;
		}

		JSONIFIER_INLINE bool hasFullBlock() const noexcept {
			return index < lengthMinusStep;
		}

		alignas(simdBytesPerRegister) uint8_t block[stepBytes]{};
		uint64_t lengthMinusStep{};
		read_buffer_ptr inString{};
		uint64_t length{};
		uint64_t index{};
	};

	struct string_block_reader_locals {
		static constexpr uint64_t stepBytes = simdBlocksPerStep * 64;

		JSONIFIER_INLINE unrestrict_t<read_buffer_ptr> getRemainder(uint64_t paddedBytes) noexcept {
			tail_block_builder::impl(+block, inString + index, length - index, paddedBytes);
			return +block;
		}

		JSONIFIER_INLINE void reset(read_buffer_ptr stringViewNew, uint64_t lengthNew) noexcept {
			lengthMinusStep = lengthNew < stepBytes ? 0 : lengthNew - stepBytes;
			inString		= std::bit_cast<read_buffer_ptr>(stringViewNew);
			length			= lengthNew;
			index			= 0;
		}

		alignas(simdBytesPerRegister) uint8_t block[stepBytes]{};
		read_buffer_ptr inString{};
		uint64_t lengthMinusStep{};
		uint64_t length{};
		uint64_t index{};
	};

	template<uint64_t stepBytes = simdBytesPerStep> struct pod_block_reader {
		JSONIFIER_INLINE unrestrict_t<read_buffer_ptr> getRemainder() noexcept {
			const uint64_t remaining = lengthVal - indexVal;
			read_buffer_ptr srcPtr	 = inString + indexVal;
			if (tail_block_builder::readStaysInPage(srcPtr, remaining, simdBytesPerBlock)) [[likely]] {
				tail_block_builder::buildInPlace(+block, srcPtr, remaining, simdBytesPerBlock);
			} else {
				jsonifierMemcpyUpTo<simdBytesPerBlock - 1>(+block, srcPtr, remaining);
				std::memset(+block + remaining, static_cast<uint8_t>(0x20), simdBytesPerBlock - remaining);
			}
			return +block;
		}

		JSONIFIER_INLINE void reset(read_buffer_ptr stringViewNew, uint64_t lengthNew) noexcept {
			fullStepEnd	 = lengthNew & ~(stepBytes - 1ull);
			fullBlockEnd = lengthNew & ~(simdBytesPerBlock - 1ull);
			inString	 = std::bit_cast<read_buffer_ptr>(stringViewNew);
			lengthVal	 = lengthNew;
			indexVal	 = 0;
		}

		JSONIFIER_INLINE unrestrict_t<read_buffer_ptr> fullBlock() noexcept {
			read_buffer_ptr newPtr = inString + indexVal;
			indexVal += simdBytesPerBlock;
			return newPtr;
		}

		JSONIFIER_INLINE unrestrict_t<read_buffer_ptr> fullStep() noexcept {
			read_buffer_ptr newPtr = inString + indexVal;
			indexVal += stepBytes;
			return newPtr;
		}

		JSONIFIER_INLINE uint64_t getRemainderBytes() const noexcept {
			return lengthVal - indexVal;
		}

		JSONIFIER_INLINE bool hasFullBlock() const noexcept {
			return indexVal < fullBlockEnd;
		}

		JSONIFIER_INLINE bool hasFullStep() const noexcept {
			return indexVal < fullStepEnd;
		}

		JSONIFIER_INLINE uint64_t length() const noexcept {
			return lengthVal;
		}

		JSONIFIER_INLINE uint64_t index() const noexcept {
			return indexVal;
		}

		alignas(simdBytesPerRegister) uint8_t block[simdBytesPerBlock];
		read_buffer_ptr inString{};
		uint64_t fullBlockEnd{};
		uint64_t fullStepEnd{};
		uint64_t lengthVal{};
		uint64_t indexVal{};

		static_assert((stepBytes & (stepBytes - 1)) == 0 && stepBytes >= simdBytesPerBlock);
	};

	struct rope_block {
		JSONIFIER_INLINE uint64_t nonQuoteOutsideString(uint64_t mask) const noexcept {
			return mask & ~inString;
		}

		JSONIFIER_INLINE uint64_t stringTail() const noexcept {
			return inString ^ quotes;
		}

		uint64_t inString{};
		uint64_t escaped{};
		uint64_t quotes{};
	};

	struct simd_string_reader_members : simd::rope_detector<rope_block>, string_block_reader, add_tape_values<make_integer_sequence<simdBlocksPerStep>>, alloc_wrapper<uint32_t> {
		friend add_tape_values<make_integer_sequence<simdBlocksPerStep>>;
		using allocator = alloc_wrapper<uint32_t>;

		JSONIFIER_INLINE simd_string_reader_members() noexcept = default;

		JSONIFIER_INLINE simd_string_reader_members& operator=(simd_string_reader_members&& other) noexcept {
			if (&other != this) {
				swap(tapeCount, other.tapeCount);
				swap(capacity, other.capacity);
				swap(string_block_reader::length, other.string_block_reader::length);
				swap(tape, other.tape);
			}
			return *this;
		}

		JSONIFIER_INLINE simd_string_reader_members(simd_string_reader_members&& other) noexcept : allocator{} {
			*this = internal::move(other);
		}

		simd_string_reader_members& operator=(const simd_string_reader_members& other) = delete;
		simd_string_reader_members(const simd_string_reader_members& other)			   = delete;

		template<bool minified> JSONIFIER_INLINE void reset(read_buffer_ptr rootIter, uint64_t stringLength) noexcept {
			const uint64_t neededCapacity = stringLength + 64;
			if (neededCapacity > capacity) {
				auto newTape = allocator::allocate(neededCapacity);
				if (tape) {
					allocator::deallocate(tape, capacity);
				}
				tape	 = newTape;
				capacity = neededCapacity;
			}

			tapeCount = 0;
			string_block_reader::reset(rootIter, stringLength);
			simd::rope_detector<rope_block>::prevInString  = 0;
			simd::rope_detector<rope_block>::prevScalar	   = 0;
			simd::rope_detector<rope_block>::nextIsEscaped = 0;

			const jsonifier_simd_int_t bsRegister	 = simd::gatherValue<jsonifier_simd_int_t>('\\');
			const jsonifier_simd_int_t quoteRegister = simd::gatherValue<jsonifier_simd_int_t>('"');
			const jsonifier_simd_int_t opTable		 = simd::gatherValues<jsonifier_simd_int_t>(simd::opArray<simdBytesPerRegister>.data());
			const jsonifier_simd_int_t spaceMask	 = simd::gatherValue<jsonifier_simd_int_t>(static_cast<char>(0x20));

			if constexpr (minified) {
				resetImpl<minified>(bsRegister, quoteRegister, opTable, spaceMask);
			} else {
				const jsonifier_simd_int_t whitespaceTableLocal = simd::gatherValues<jsonifier_simd_int_t>(simd::whitespaceArray<simdBytesPerRegister>.data());
				resetImpl<minified>(bsRegister, quoteRegister, opTable, spaceMask, whitespaceTableLocal);
			}
		}

		JSONIFIER_INLINE unrestrict_t<write_structural_index_ptr> begin() noexcept {
			tape[tapeCount] = static_cast<uint32_t>(string_block_reader::length);
			return tape;
		}

		JSONIFIER_INLINE unrestrict_t<write_structural_index_ptr> end() noexcept {
			return tape + tapeCount;
		}

		JSONIFIER_INLINE uint64_t getTapeCount() noexcept {
			return tapeCount;
		}

		JSONIFIER_INLINE ~simd_string_reader_members() noexcept {
			if (tape) {
				allocator::deallocate(tape, capacity);
				tape = nullptr;
			}
		}

	  protected:
		template<typename... jsonifier_simd_int_types> JSONIFIER_INLINE void processBlocks(read_buffer_ptr blockPtr, uint64_t stepBaseIndex, const jsonifier_simd_int_t bsRegister,
			const jsonifier_simd_int_t quoteRegister, const jsonifier_simd_int_t opTable, const jsonifier_simd_int_t spaceMask, const jsonifier_simd_int_types... args) noexcept {
			array<uint64_t, simdBlocksPerStep> bitsArr;
			array<uint64_t, simdBlocksPerStep> cntsArr;
			processBlocksImpl<0>(bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
			if constexpr (simdBlocksPerStep > 1) {
				processBlocksImpl<1>(bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
				if constexpr (simdBlocksPerStep > 2) {
					processBlocksImpl<2>(bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
					processBlocksImpl<3>(bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
					if constexpr (simdBlocksPerStep > 4) {
						processBlocksImpl<4>(bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
						processBlocksImpl<5>(bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
						processBlocksImpl<6>(bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
						processBlocksImpl<7>(bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
					}
				}
			}

			add_tape_values<make_integer_sequence<simdBlocksPerStep>>::impl(bitsArr, cntsArr, tape + tapeCount, stepBaseIndex);

			tapeCount += cntsArr[0];
			if constexpr (simdBlocksPerStep > 1) {
				tapeCount += cntsArr[1];
				if constexpr (simdBlocksPerStep > 2) {
					tapeCount += cntsArr[2];
					tapeCount += cntsArr[3];
					if constexpr (simdBlocksPerStep > 4) {
						tapeCount += cntsArr[4];
						tapeCount += cntsArr[5];
						tapeCount += cntsArr[6];
						tapeCount += cntsArr[7];
					}
				}
			}
		}

		template<bool minified, typename... jsonifier_simd_int_types> JSONIFIER_INLINE void resetImpl(const jsonifier_simd_int_t bsRegister,
			const jsonifier_simd_int_t quoteRegister, const jsonifier_simd_int_t opTable, const jsonifier_simd_int_t spaceMask, const jsonifier_simd_int_types... args) noexcept {
			while (string_block_reader::hasFullBlock()) {
				const uint64_t stepBaseIndex = string_block_reader::index;
				processBlocks(string_block_reader::fullBlock(), stepBaseIndex, bsRegister, quoteRegister, opTable, spaceMask, args...);
			}

			if (const uint64_t remaining = string_block_reader::getRemainderBytes(); remaining != 0) {
				const uint64_t tailBlocks = (remaining + 63) / 64;
				read_buffer_ptr tailPtr	  = string_block_reader::getRemainder(tailBlocks * 64);
				for (uint64_t blockIndex = 0; blockIndex < tailBlocks; ++blockIndex) {
					processTailBlock(tailPtr + blockIndex * 64, string_block_reader::index + blockIndex * 64, bsRegister, quoteRegister, opTable, spaceMask, args...);
				}
				while (tapeCount > 0 && tape[tapeCount - 1] >= string_block_reader::length) {
					--tapeCount;
				}
			}
		}

		template<uint64_t currentBlock, uint64_t laneCount, typename... jsonifier_simd_int_types> JSONIFIER_INLINE void processBlocksImpl(array<uint64_t, laneCount>& bitsArr,
			array<uint64_t, laneCount>& cntsArr, read_buffer_ptr blockPtr, const jsonifier_simd_int_t bsRegister, const jsonifier_simd_int_t quoteRegister,
			const jsonifier_simd_int_t opTable, const jsonifier_simd_int_t spaceMask, const jsonifier_simd_int_types... args) noexcept {
			simd_array_t inVals;
			inVals.template set<0>(simd::gatherValuesU<jsonifier_simd_int_t>(blockPtr + currentBlock * 64));
			if constexpr (simdRegistersPerBlock > 1) {
				inVals.template set<1>(simd::gatherValuesU<jsonifier_simd_int_t>(blockPtr + currentBlock * 64 + simdBytesPerRegister * 1));
				if constexpr (simdRegistersPerBlock > 2) {
					inVals.template set<2>(simd::gatherValuesU<jsonifier_simd_int_t>(blockPtr + currentBlock * 64 + simdBytesPerRegister * 2));
					inVals.template set<3>(simd::gatherValuesU<jsonifier_simd_int_t>(blockPtr + currentBlock * 64 + simdBytesPerRegister * 3));
				}
			}
			simd::rope_detector<rope_block>::next(inVals, bsRegister, quoteRegister);
			const uint64_t structurals = getStructurals(inVals, opTable, spaceMask, args...) & ~simd::rope_detector<rope_block>::stringTail();
			bitsArr[currentBlock]	   = structurals;
			cntsArr[currentBlock]	   = simd::tape_writer_op::correctedPopcount(structurals);
		}

		template<typename... jsonifier_simd_int_types> JSONIFIER_INLINE void processTailBlock(read_buffer_ptr blockPtr, uint64_t blockBaseIndex,
			const jsonifier_simd_int_t bsRegister, const jsonifier_simd_int_t quoteRegister, const jsonifier_simd_int_t opTable, const jsonifier_simd_int_t spaceMask,
			const jsonifier_simd_int_types... args) noexcept {
			array<uint64_t, 1> bitsArr;
			array<uint64_t, 1> cntsArr;
			processBlocksImpl<0>(bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
			add_tape_values<make_integer_sequence<1>>::impl(bitsArr, cntsArr, tape + tapeCount, blockBaseIndex);
			tapeCount += cntsArr[0];
		}

		JSONIFIER_INLINE uint64_t getStructurals(const simd_array_t in01, const jsonifier_simd_int_t opTable, const jsonifier_simd_int_t spaceMask) noexcept {
			const uint64_t op		   = simd::op_collector::impl(in01, opTable, spaceMask);
			const uint64_t scalar	   = ~(op | simd::rope_detector<rope_block>::quotes);
			const uint64_t follows	   = simd::rope_detector<rope_block>::followsNonquoteScalar(scalar);
			const uint64_t scalarStart = scalar & ~follows;
			return op | simd::rope_detector<rope_block>::quotes | scalarStart;
		}

		JSONIFIER_INLINE uint64_t getStructurals(const simd_array_t in01, const jsonifier_simd_int_t opTable, const jsonifier_simd_int_t spaceMask,
			const jsonifier_simd_int_t whitespaceTableLocal) noexcept {
#if JSONIFIER_NEON
			const uint64x2_t wsOp	  = simd::ws_op_collector::impl(in01, opTable, spaceMask, whitespaceTableLocal);
			const uint64_t whitespace = vgetq_lane_u64(wsOp, 0);
			const uint64_t op		  = vgetq_lane_u64(wsOp, 1);
#else
			const uint64_t whitespace = simd::ws_collector::impl(in01, whitespaceTableLocal);
			const uint64_t op		  = simd::op_collector::impl(in01, opTable, spaceMask);
#endif
			const uint64_t scalar	   = ~(op | whitespace | simd::rope_detector<rope_block>::quotes);
			const uint64_t follows	   = simd::rope_detector<rope_block>::followsNonquoteScalar(scalar);
			const uint64_t scalarStart = scalar & ~follows;
			return op | simd::rope_detector<rope_block>::quotes | scalarStart;
		}

		write_structural_index_ptr tape{};
		uint64_t tapeCount{};
		uint64_t capacity{};
	};

	struct simd_string_reader_locals : string_block_reader_locals, add_tape_values<make_integer_sequence<simdBlocksPerStep>>, alloc_wrapper<uint32_t> {
		friend add_tape_values<make_integer_sequence<simdBlocksPerStep>>;
		using allocator = alloc_wrapper<uint32_t>;
		using rope_type = simd::rope_detector<rope_block>;

		JSONIFIER_INLINE simd_string_reader_locals() noexcept = default;

		JSONIFIER_INLINE simd_string_reader_locals& operator=(simd_string_reader_locals&& other) noexcept {
			if (&other != this) {
				swap(tapeCount, other.tapeCount);
				swap(capacity, other.capacity);
				swap(string_block_reader_locals::length, other.string_block_reader_locals::length);
				swap(tape, other.tape);
			}
			return *this;
		}

		JSONIFIER_INLINE simd_string_reader_locals(simd_string_reader_locals&& other) noexcept : allocator{} {
			*this = internal::move(other);
		}

		simd_string_reader_locals& operator=(const simd_string_reader_locals& other) = delete;
		simd_string_reader_locals(const simd_string_reader_locals& other)			 = delete;

#if JSONIFIER_COMPILER_CLANG && JSONIFIER_NEON
		template<uint64_t offset> JSONIFIER_INLINE static void collectCountsQuad(const array<uint64_t, simdBlocksPerStep>& __restrict bitsArr,
			array<uint64_t, simdBlocksPerStep>& __restrict cntsArr) noexcept {
			const uint8x16_t counts01 = vcntq_u8(vreinterpretq_u8_u64(vcombine_u64(vcreate_u64(bitsArr[offset]), vcreate_u64(bitsArr[offset + 1]))));
			const uint8x16_t counts23 = vcntq_u8(vreinterpretq_u8_u64(vcombine_u64(vcreate_u64(bitsArr[offset + 2]), vcreate_u64(bitsArr[offset + 3]))));
			uint8x16_t sums			  = vpaddq_u8(counts01, counts23);
			sums					  = vpaddq_u8(sums, sums);
			sums					  = vpaddq_u8(sums, sums);
			const uint32_t packed	  = vgetq_lane_u32(vreinterpretq_u32_u8(sums), 0);
			cntsArr[offset]			  = packed & 0xFFu;
			cntsArr[offset + 1]		  = (packed >> 8) & 0xFFu;
			cntsArr[offset + 2]		  = (packed >> 16) & 0xFFu;
			cntsArr[offset + 3]		  = packed >> 24;
		}

		JSONIFIER_INLINE static void collectCounts(const array<uint64_t, simdBlocksPerStep>& __restrict bitsArr, array<uint64_t, simdBlocksPerStep>& __restrict cntsArr) noexcept {
			collectCountsQuad<0>(bitsArr, cntsArr);
			if constexpr (simdBlocksPerStep > 4) {
				collectCountsQuad<4>(bitsArr, cntsArr);
			}
		}

		static_assert(simdBlocksPerStep == 4 || simdBlocksPerStep == 8);
#endif

		template<uint64_t currentBlock, uint64_t laneCount, typename... jsonifier_simd_int_types> JSONIFIER_INLINE static void processBlocksImpl(rope_type& __restrict rope,
			array<uint64_t, laneCount>& __restrict bitsArr, array<uint64_t, laneCount>& __restrict cntsArr, read_buffer_ptr blockPtr, const jsonifier_simd_int_t bsRegister,
			const jsonifier_simd_int_t quoteRegister, const jsonifier_simd_int_t opTable, const jsonifier_simd_int_t spaceMask, const jsonifier_simd_int_types... args) noexcept {
			simd_array_t inVals;
			inVals.template set<0>(simd::gatherValuesU<jsonifier_simd_int_t>(blockPtr + currentBlock * 64));
			if constexpr (simdRegistersPerBlock > 1) {
				inVals.template set<1>(simd::gatherValuesU<jsonifier_simd_int_t>(blockPtr + currentBlock * 64 + simdBytesPerRegister * 1));
				if constexpr (simdRegistersPerBlock > 2) {
					inVals.template set<2>(simd::gatherValuesU<jsonifier_simd_int_t>(blockPtr + currentBlock * 64 + simdBytesPerRegister * 2));
					inVals.template set<3>(simd::gatherValuesU<jsonifier_simd_int_t>(blockPtr + currentBlock * 64 + simdBytesPerRegister * 3));
				}
			}
			rope.next(inVals, bsRegister, quoteRegister);
			const uint64_t structurals = getStructurals(rope, inVals, opTable, spaceMask, args...) & ~rope.stringTail();
			bitsArr[currentBlock]	   = structurals;
#if JSONIFIER_COMPILER_CLANG && JSONIFIER_NEON
			if constexpr (laneCount == 1) {
				cntsArr[currentBlock] = simd::tape_writer_op::correctedPopcount(structurals);
			}
#else
			cntsArr[currentBlock] = simd::tape_writer_op::correctedPopcount(structurals);
#endif
		}

		template<bool minified, typename... jsonifier_simd_int_types> JSONIFIER_INLINE void resetImpl(const jsonifier_simd_int_t bsRegister,
			const jsonifier_simd_int_t quoteRegister, const jsonifier_simd_int_t opTable, const jsonifier_simd_int_t spaceMask, const jsonifier_simd_int_types... args) noexcept {
			rope_type rope{};
			write_structural_index_ptr tapeLocal = tape;
			read_buffer_ptr srcPtr				 = string_block_reader_locals::inString;
			const uint64_t lengthMinusStepLocal	 = string_block_reader_locals::lengthMinusStep;
			const uint64_t lengthLocal			 = string_block_reader_locals::length;
			uint64_t tapeCountLocal				 = 0;
			uint64_t indexLocal					 = 0;

			while (indexLocal < lengthMinusStepLocal) {
				tapeCountLocal += processBlocks(rope, srcPtr + indexLocal, tapeLocal + tapeCountLocal, indexLocal, bsRegister, quoteRegister, opTable, spaceMask, args...);
				indexLocal += stepBytes;
			}
			string_block_reader_locals::index = indexLocal;

			if (const uint64_t remaining = lengthLocal - indexLocal; remaining != 0) {
				const uint64_t tailBlocks = (remaining + 63) / 64;
				read_buffer_ptr tailPtr	  = string_block_reader_locals::getRemainder(tailBlocks * 64);
				for (uint64_t blockIndex = 0; blockIndex < tailBlocks; ++blockIndex) {
					tapeCountLocal += processTailBlock(rope, tailPtr + blockIndex * 64, tapeLocal + tapeCountLocal, indexLocal + blockIndex * 64, bsRegister, quoteRegister,
						opTable, spaceMask, args...);
				}
				while (tapeCountLocal > 0 && tapeLocal[tapeCountLocal - 1] >= lengthLocal) {
					--tapeCountLocal;
				}
			}
			tapeCount = tapeCountLocal;
		}

		template<typename... jsonifier_simd_int_types> JSONIFIER_INLINE static uint64_t processTailBlock(rope_type& __restrict rope, read_buffer_ptr blockPtr,
			write_structural_index_ptr tapePtr, uint64_t blockBaseIndex, const jsonifier_simd_int_t bsRegister, const jsonifier_simd_int_t quoteRegister,
			const jsonifier_simd_int_t opTable, const jsonifier_simd_int_t spaceMask, const jsonifier_simd_int_types... args) noexcept {
			array<uint64_t, 1> bitsArr;
			array<uint64_t, 1> cntsArr;
			processBlocksImpl<0>(rope, bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
			add_tape_values<make_integer_sequence<1>>::impl(bitsArr, cntsArr, tapePtr, blockBaseIndex);
			return cntsArr[0];
		}

		template<typename... jsonifier_simd_int_types> JSONIFIER_INLINE static uint64_t processBlocks(rope_type& __restrict rope, read_buffer_ptr blockPtr,
			write_structural_index_ptr tapePtr, uint64_t stepBaseIndex, const jsonifier_simd_int_t bsRegister, const jsonifier_simd_int_t quoteRegister,
			const jsonifier_simd_int_t opTable, const jsonifier_simd_int_t spaceMask, const jsonifier_simd_int_types... args) noexcept {
			array<uint64_t, simdBlocksPerStep> bitsArr;
			array<uint64_t, simdBlocksPerStep> cntsArr;
			processBlocksImpl<0>(rope, bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
			if constexpr (simdBlocksPerStep > 1) {
				processBlocksImpl<1>(rope, bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
				if constexpr (simdBlocksPerStep > 2) {
					processBlocksImpl<2>(rope, bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
					processBlocksImpl<3>(rope, bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
					if constexpr (simdBlocksPerStep > 4) {
						processBlocksImpl<4>(rope, bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
						processBlocksImpl<5>(rope, bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
						processBlocksImpl<6>(rope, bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
						processBlocksImpl<7>(rope, bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
					}
				}
			}
#if JSONIFIER_COMPILER_CLANG && JSONIFIER_NEON
			collectCounts(bitsArr, cntsArr);
#endif

			add_tape_values<make_integer_sequence<simdBlocksPerStep>>::impl(bitsArr, cntsArr, tapePtr, stepBaseIndex);

			uint64_t stepCount = cntsArr[0];
			if constexpr (simdBlocksPerStep > 1) {
				stepCount += cntsArr[1];
				if constexpr (simdBlocksPerStep > 2) {
					stepCount += cntsArr[2];
					stepCount += cntsArr[3];
					if constexpr (simdBlocksPerStep > 4) {
						stepCount += cntsArr[4];
						stepCount += cntsArr[5];
						stepCount += cntsArr[6];
						stepCount += cntsArr[7];
					}
				}
			}
			return stepCount;
		}

		template<bool minified> JSONIFIER_INLINE void reset(read_buffer_ptr rootIter, uint64_t stringLength) noexcept {
			const uint64_t neededCapacity = stringLength + 64;
			if (neededCapacity > capacity) {
				auto newTape = allocator::allocate(neededCapacity);
				if (tape) {
					allocator::deallocate(tape, capacity);
				}
				tape	 = newTape;
				capacity = neededCapacity;
			}

			string_block_reader_locals::reset(rootIter, stringLength);

			const jsonifier_simd_int_t bsRegister	 = simd::gatherValue<jsonifier_simd_int_t>('\\');
			const jsonifier_simd_int_t quoteRegister = simd::gatherValue<jsonifier_simd_int_t>('"');
			const jsonifier_simd_int_t opTable		 = simd::gatherValues<jsonifier_simd_int_t>(simd::opArray<simdBytesPerRegister>.data());
			const jsonifier_simd_int_t spaceMask	 = simd::gatherValue<jsonifier_simd_int_t>(static_cast<char>(0x20));

			if constexpr (minified) {
				resetImpl<minified>(bsRegister, quoteRegister, opTable, spaceMask);
			} else {
				const jsonifier_simd_int_t whitespaceTableLocal = simd::gatherValues<jsonifier_simd_int_t>(simd::whitespaceArray<simdBytesPerRegister>.data());
				resetImpl<minified>(bsRegister, quoteRegister, opTable, spaceMask, whitespaceTableLocal);
			}
		}

		JSONIFIER_INLINE static uint64_t getStructurals(rope_type& __restrict rope, const simd_array_t in01, const jsonifier_simd_int_t opTable,
			const jsonifier_simd_int_t spaceMask, const jsonifier_simd_int_t whitespaceTableLocal) noexcept {
#if JSONIFIER_NEON
			const uint64x2_t wsOp	  = simd::ws_op_collector::impl(in01, opTable, spaceMask, whitespaceTableLocal);
			const uint64_t whitespace = vgetq_lane_u64(wsOp, 0);
			const uint64_t op		  = vgetq_lane_u64(wsOp, 1);
#else
			const uint64_t whitespace = simd::ws_collector::impl(in01, whitespaceTableLocal);
			const uint64_t op		  = simd::op_collector::impl(in01, opTable, spaceMask);
#endif
			const uint64_t quotes	   = rope.quotes;
			const uint64_t scalar	   = ~(op | whitespace | quotes);
			const uint64_t follows	   = rope.followsNonquoteScalar(scalar);
			const uint64_t scalarStart = scalar & ~follows;
			return op | quotes | scalarStart;
		}

		JSONIFIER_INLINE static uint64_t getStructurals(rope_type& __restrict rope, const simd_array_t in01, const jsonifier_simd_int_t opTable,
			const jsonifier_simd_int_t spaceMask) noexcept {
			const uint64_t op		   = simd::op_collector::impl(in01, opTable, spaceMask);
			const uint64_t quotes	   = rope.quotes;
			const uint64_t scalar	   = ~(op | quotes);
			const uint64_t follows	   = rope.followsNonquoteScalar(scalar);
			const uint64_t scalarStart = scalar & ~follows;
			return op | quotes | scalarStart;
		}

		JSONIFIER_INLINE unrestrict_t<write_structural_index_ptr> begin() noexcept {
			tape[tapeCount] = static_cast<uint32_t>(string_block_reader_locals::length);
			return tape;
		}

		JSONIFIER_INLINE unrestrict_t<write_structural_index_ptr> end() noexcept {
			return tape + tapeCount;
		}

		JSONIFIER_INLINE uint64_t getTapeCount() noexcept {
			return tapeCount;
		}

		JSONIFIER_INLINE ~simd_string_reader_locals() noexcept {
			if (tape) {
				allocator::deallocate(tape, capacity);
				tape = nullptr;
			}
		}

		write_structural_index_ptr tape{};
		uint64_t tapeCount{};
		uint64_t capacity{};
	};

#if JSONIFIER_COMPILER_CLANG
	using simd_string_reader = simd_string_reader_members;
#else
	using simd_string_reader = simd_string_reader_locals;
#endif

	struct pod_simd_string_reader_members : simd::rope_detector<rope_block>, alloc_wrapper<uint32_t> {
		using allocator = alloc_wrapper<uint32_t>;
		using rope_type = simd::rope_detector<rope_block>;
		static constexpr uint64_t stepBytes{ podBytesPerStep };

		JSONIFIER_INLINE pod_simd_string_reader_members() noexcept = default;

		JSONIFIER_INLINE pod_simd_string_reader_members& operator=(pod_simd_string_reader_members&& other) noexcept {
			if (&other != this) {
				swap(tapeCount, other.tapeCount);
				swap(capacity, other.capacity);
				swap(length, other.length);
				swap(tape, other.tape);
			}
			return *this;
		}

		JSONIFIER_INLINE pod_simd_string_reader_members(pod_simd_string_reader_members&& other) noexcept : allocator{} {
			*this = internal::move(other);
		}

		pod_simd_string_reader_members& operator=(const pod_simd_string_reader_members& other) = delete;
		pod_simd_string_reader_members(const pod_simd_string_reader_members& other)			   = delete;

		template<uint64_t currentBlock, uint64_t registerBytes, uint64_t registerCount, uint64_t blocksPerStep, typename... simd_types>
		JSONIFIER_INLINE void processBlocksImpl(array<uint64_t, blocksPerStep>& __restrict bitsArr, array<uint64_t, blocksPerStep>& __restrict cntsArr, read_buffer_ptr blockPtr,
			const simd_register_t<registerBytes> bsRegister, const simd_register_t<registerBytes> quoteRegister, const simd_register_t<registerBytes> opTable,
			const simd_register_t<registerBytes> spaceMask, const simd_types... args) noexcept {
			using simd_type = simd_register_t<registerBytes>;
			pod_simd_array_t<registerCount, registerBytes> inVals;
			inVals.template set<0>(simd::gatherValuesU<simd_type>(blockPtr + currentBlock * simdBytesPerBlock));
			if constexpr (registerCount > 1) {
				inVals.template set<1>(simd::gatherValuesU<simd_type>(blockPtr + currentBlock * simdBytesPerBlock + registerBytes * 1));
				if constexpr (registerCount > 2) {
					inVals.template set<2>(simd::gatherValuesU<simd_type>(blockPtr + currentBlock * simdBytesPerBlock + registerBytes * 2));
					inVals.template set<3>(simd::gatherValuesU<simd_type>(blockPtr + currentBlock * simdBytesPerBlock + registerBytes * 3));
				}
			}
			rope_type::template nextScalar<registerBytes, registerCount>(inVals, bsRegister, quoteRegister);
			const uint64_t structurals = getStructurals<registerBytes, registerCount>(inVals, opTable, spaceMask, args...) & ~rope_type::stringTail();
			bitsArr[currentBlock]	   = structurals;
			cntsArr[currentBlock]	   = simd::tape_writer_op::correctedPopcount(structurals);
		}

		template<bool minified, uint64_t registerBytes, uint64_t registerCount> JSONIFIER_INLINE void resetDispatch(read_buffer_ptr rootIter, uint64_t stringLength) noexcept {
			using simd_type				  = simd_register_t<registerBytes>;
			const uint64_t neededCapacity = stringLength + 64;
			if (neededCapacity > capacity) {
				auto newTape = allocator::allocate(neededCapacity);
				if (tape) {
					allocator::deallocate(tape, capacity);
				}
				tape	 = newTape;
				capacity = neededCapacity;
			}

			const simd_type bsRegister	  = simd::gatherValue<simd_type>('\\');
			const simd_type quoteRegister = simd::gatherValue<simd_type>('"');
			const simd_type opTable		  = simd::gatherValues<simd_type>(simd::opArray<registerBytes>.data());
			const simd_type spaceMask	  = simd::gatherValue<simd_type>(static_cast<char>(0x20));

			if constexpr (minified) {
				resetImpl<registerBytes, registerCount>(rootIter, stringLength, bsRegister, quoteRegister, opTable, spaceMask);
			} else {
				const simd_type whitespaceTableLocal = simd::gatherValues<simd_type>(simd::whitespaceArray<registerBytes>.data());
				resetImpl<registerBytes, registerCount>(rootIter, stringLength, bsRegister, quoteRegister, opTable, spaceMask, whitespaceTableLocal);
			}
		}

		template<uint64_t blocksPerStep, uint64_t registerBytes, uint64_t registerCount, typename... simd_types> JSONIFIER_INLINE void processBlocks(read_buffer_ptr blockPtr,
			uint64_t stepBaseIndex, const simd_register_t<registerBytes> bsRegister, const simd_register_t<registerBytes> quoteRegister,
			const simd_register_t<registerBytes> opTable, const simd_register_t<registerBytes> spaceMask, const simd_types... args) noexcept {
			array<uint64_t, blocksPerStep> bitsArr;
			array<uint64_t, blocksPerStep> cntsArr;
			processBlocksImpl<0, registerBytes, registerCount>(bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
			if constexpr (blocksPerStep > 1) {
				processBlocksImpl<1, registerBytes, registerCount>(bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
				if constexpr (blocksPerStep > 2) {
					processBlocksImpl<2, registerBytes, registerCount>(bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
					processBlocksImpl<3, registerBytes, registerCount>(bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
					if constexpr (blocksPerStep > 4) {
						processBlocksImpl<4, registerBytes, registerCount>(bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
						processBlocksImpl<5, registerBytes, registerCount>(bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
						processBlocksImpl<6, registerBytes, registerCount>(bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
						processBlocksImpl<7, registerBytes, registerCount>(bitsArr, cntsArr, blockPtr, bsRegister, quoteRegister, opTable, spaceMask, args...);
					}
				}
			}

			add_tape_values<make_integer_sequence<blocksPerStep>>::impl(bitsArr, cntsArr, tape + tapeCount, stepBaseIndex);

			tapeCount += cntsArr[0];
			if constexpr (blocksPerStep > 1) {
				tapeCount += cntsArr[1];
				if constexpr (blocksPerStep > 2) {
					tapeCount += cntsArr[2];
					tapeCount += cntsArr[3];
					if constexpr (blocksPerStep > 4) {
						tapeCount += cntsArr[4];
						tapeCount += cntsArr[5];
						tapeCount += cntsArr[6];
						tapeCount += cntsArr[7];
					}
				}
			}
		}

		template<uint64_t registerBytes, uint64_t registerCount> JSONIFIER_INLINE uint64_t getStructurals(const pod_simd_array_t<registerCount, registerBytes> in01,
			const simd_register_t<registerBytes> opTable, const simd_register_t<registerBytes> spaceMask, const simd_register_t<registerBytes> whitespaceTableLocal) noexcept {
			const uint64_t whitespace  = simd::pod_ws_collector<registerBytes, registerCount>::impl(in01, whitespaceTableLocal);
			const uint64_t op		   = simd::scalar_op_collector<registerBytes, registerCount>::impl(in01, opTable, spaceMask);
			const uint64_t scalar	   = ~(op | whitespace | rope_type::quotes);
			const uint64_t follows	   = rope_type::followsNonquoteScalar(scalar);
			const uint64_t scalarStart = scalar & ~follows;
			return op | rope_type::quotes | scalarStart;
		}

		template<uint64_t registerBytes, uint64_t registerCount, typename... simd_types> JSONIFIER_INLINE void resetImpl(read_buffer_ptr rootIter, uint64_t stringLength,
			const simd_register_t<registerBytes> bsRegister, const simd_register_t<registerBytes> quoteRegister, const simd_register_t<registerBytes> opTable,
			const simd_register_t<registerBytes> spaceMask, const simd_types... args) noexcept {
			static constexpr uint64_t blocksPerStep{ stepBytes / simdBytesPerBlock };
			stringBlockReader.reset(rootIter, stringLength);
			rope_type::prevInString	 = 0;
			rope_type::prevScalar	 = 0;
			rope_type::nextIsEscaped = 0;
			tapeCount				 = 0;
			if constexpr (registerCount * registerBytes == simdBytesPerBlock) {
				while (stringBlockReader.hasFullStep()) {
					const uint64_t stepBaseIndex = stringBlockReader.index();
					processBlocks<blocksPerStep, registerBytes, registerCount>(stringBlockReader.fullStep(), stepBaseIndex, bsRegister, quoteRegister, opTable, spaceMask, args...);
				}

				while (stringBlockReader.hasFullBlock()) {
					const uint64_t blockBaseIndex = stringBlockReader.index();
					processBlocks<1, registerBytes, registerCount>(stringBlockReader.fullBlock(), blockBaseIndex, bsRegister, quoteRegister, opTable, spaceMask, args...);
				}
			}

			if (stringBlockReader.getRemainderBytes() != 0) {
				const uint64_t blockBaseIndex = stringBlockReader.index();
				processBlocks<1, registerBytes, registerCount>(stringBlockReader.getRemainder(), blockBaseIndex, bsRegister, quoteRegister, opTable, spaceMask, args...);
				while (tapeCount > 0 && tape[tapeCount - 1] >= stringBlockReader.length()) {
					--tapeCount;
				}
			}
			length = stringBlockReader.length();
		}

		template<uint64_t registerBytes, uint64_t registerCount> JSONIFIER_INLINE uint64_t getStructurals(const pod_simd_array_t<registerCount, registerBytes> in01,
			const simd_register_t<registerBytes> opTable, const simd_register_t<registerBytes> spaceMask) noexcept {
			const uint64_t op		   = simd::scalar_op_collector<registerBytes, registerCount>::impl(in01, opTable, spaceMask);
			const uint64_t scalar	   = ~(op | rope_type::quotes);
			const uint64_t follows	   = rope_type::followsNonquoteScalar(scalar);
			const uint64_t scalarStart = scalar & ~follows;
			return op | rope_type::quotes | scalarStart;
		}

		template<bool minified> JSONIFIER_INLINE void reset(read_buffer_ptr rootIter, uint64_t stringLength) noexcept {
#if JSONIFIER_ANY_AVX
			if (stringLength <= 16) {
				return resetDispatch<minified, 16, 1>(rootIter, stringLength);
			}
			if constexpr (simdBytesPerRegister >= 32) {
				if (stringLength <= 32) {
					return resetDispatch<minified, 32, 1>(rootIter, stringLength);
				}
			}
#elif JSONIFIER_NEON
			if (stringLength <= 16) {
				return resetDispatch<minified, 16, 1>(rootIter, stringLength);
			}
			if (stringLength <= 32) {
				return resetDispatch<minified, 16, 2>(rootIter, stringLength);
			}
#endif
			resetDispatch<minified, simdBytesPerRegister, simdRegistersPerBlock>(rootIter, stringLength);
		}

		JSONIFIER_INLINE unrestrict_t<write_structural_index_ptr> begin() noexcept {
			tape[tapeCount] = static_cast<uint32_t>(length);
			return tape;
		}

		JSONIFIER_INLINE unrestrict_t<write_structural_index_ptr> end() noexcept {
			return tape + tapeCount;
		}

		JSONIFIER_INLINE uint64_t getTapeCount() noexcept {
			return tapeCount;
		}

		JSONIFIER_INLINE ~pod_simd_string_reader_members() noexcept {
			if (tape) {
				allocator::deallocate(tape, capacity);
				tape = nullptr;
			}
		}

		pod_block_reader<stepBytes> stringBlockReader;
		write_structural_index_ptr tape{};
		uint64_t tapeCount{};
		uint64_t capacity{};
		uint64_t length{};
	};

	using pod_simd_string_reader = pod_simd_string_reader_members;

}
