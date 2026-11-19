/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/core/fastio.hpp
 */
#pragma once

#include <jsonifier-incl/simd/bit_ops.hpp>

namespace jsonifier::internal {

	enum class stream_targets : uint8_t { stdout_target, stderr_target };

	inline uint64_t rawWrite(stream_targets stream_target, const char* data, uint64_t len) noexcept {
		if (len == 0 || data == nullptr) {
			return 0;
		}
#if JSONIFIER_PLATFORM_WINDOWS
		static constexpr DWORD handle_ids[2]{ STD_OUTPUT_HANDLE, STD_ERROR_HANDLE };
		static constexpr uint64_t max_chunk = 1ull << 30;
		HANDLE handle						= GetStdHandle(handle_ids[static_cast<uint64_t>(stream_target)]);
		if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
			return 0;
		}
		uint64_t total = 0;
		while (total < len) {
			uint64_t remaining = len - total;
			DWORD chunk		   = static_cast<DWORD>(remaining < max_chunk ? remaining : max_chunk);
			DWORD written	   = 0;
			if (!WriteFile(handle, data + total, chunk, &written, nullptr) || written == 0) {
				break;
			}
			total += written;
		}
		return total;
#else
		static constexpr int32_t stream_types[2]{ 1, 2 };
		int32_t fd	   = stream_types[static_cast<uint64_t>(stream_target)];
		uint64_t total = 0;
		while (total < len) {
			ssize_t result = write(fd, data + total, len - total);
			if (result < 0) {
				if (errno == EINTR) {
					continue;
				}
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					pollfd pfd{ fd, POLLOUT, 0 };
					if (poll(&pfd, 1, -1) < 0 && errno != EINTR) {
						break;
					}
					continue;
				}
				break;
			}
			if (result == 0) {
				break;
			}
			total += static_cast<uint64_t>(result);
		}
		return total;
#endif
	}

	alignas(64) inline constexpr uint8_t digitCounts[]{ 19, 19, 19, 19, 18, 18, 18, 17, 17, 17, 16, 16, 16, 16, 15, 15, 15, 14, 14, 14, 13, 13, 13, 13, 12, 12, 12, 11, 11, 11, 10,
		10, 10, 10, 9, 9, 9, 8, 8, 8, 7, 7, 7, 7, 6, 6, 6, 5, 5, 5, 4, 4, 4, 4, 3, 3, 3, 2, 2, 2, 1, 1, 1, 1, 1 };

	alignas(64) inline constexpr uint64_t digitCountThresholds[]{ 0ULL, 9ULL, 99ULL, 999ULL, 9999ULL, 99999ULL, 999999ULL, 9999999ULL, 99999999ULL, 999999999ULL, 9999999999ULL,
		99999999999ULL, 999999999999ULL, 9999999999999ULL, 99999999999999ULL, 999999999999999ULL, 9999999999999999ULL, 99999999999999999ULL, 999999999999999999ULL,
		9999999999999999999ULL };

	inline uint64_t fastDigitCount(const uint64_t inputValue) noexcept {
		const uint64_t originalDigitCount{ digitCounts[simd::countlZero(inputValue)] };
		return originalDigitCount + static_cast<uint64_t>(inputValue > digitCountThresholds[originalDigitCount]);
	}

	template<typename value_type> inline uint64_t writeFloat(char* dest, value_type value) noexcept {
		auto result = std::to_chars(dest, dest + 64, value);
		return static_cast<uint64_t>(result.ptr - dest);
	}

	inline char* writeUnsigned(char* bufferEnd, uint64_t value) noexcept {
		char* pos										  = bufferEnd;
		alignas(64) static constexpr char digitPairs[201] = "0001020304050607080910111213141516171819"
															"2021222324252627282930313233343536373839"
															"4041424344454647484950515253545556575859"
															"6061626364656667686970717273747576777879"
															"8081828384858687888990919293949596979899";
		while (value >= 100) {
			auto pair = value % 100;
			value /= 100;
			pos -= 2;
			pos[0] = digitPairs[pair * 2];
			pos[1] = digitPairs[pair * 2 + 1];
		}
		if (value < 10) {
			*--pos = static_cast<char>('0' + value);
		} else {
			pos -= 2;
			pos[0] = digitPairs[value * 2];
			pos[1] = digitPairs[value * 2 + 1];
		}
		return pos;
	}

	template<typename value_type> inline uint64_t writeInteger(char* dest, value_type value) noexcept {
		if constexpr (std::is_signed_v<value_type>) {
			uint64_t magnitude;
			char* outLocal = dest;
			if (value < 0) {
				*outLocal++ = '-';
				magnitude	= ~static_cast<uint64_t>(value) + 1;
			} else {
				magnitude = static_cast<uint64_t>(value);
			}
			auto count = fastDigitCount(magnitude);
			char* end  = outLocal + count;
			writeUnsigned(end, magnitude);
			return static_cast<uint64_t>(end - dest);
		} else {
			uint64_t magnitude = static_cast<uint64_t>(value);
			auto count		   = fastDigitCount(magnitude);
			char* end		   = dest + count;
			writeUnsigned(end, magnitude);
			return count;
		}
	}

	struct endl_t {};
	inline constexpr endl_t endl{};

	struct flush_t {};
	inline constexpr flush_t flush{};

	template<uint64_t bufferSize = 8192> struct basic_stream {
		inline explicit basic_stream(stream_targets streamTargetNew) noexcept : bufferLength{ 0 }, stream_target{ streamTargetNew } {
		}

		basic_stream& operator=(const basic_stream&) = delete;
		basic_stream(const basic_stream&)			 = delete;

		template<typename integer_type>
		inline std::enable_if_t<std::is_integral_v<integer_type> && !std::is_same_v<integer_type, char> && !std::is_same_v<integer_type, bool>, basic_stream&> operator<<(
			integer_type value) noexcept {
			ensureSpace(21);
			bufferLength += writeInteger(streamBuffer + bufferLength, value);
			return *this;
		}

		template<typename float_type> inline std::enable_if_t<std::is_floating_point_v<float_type>, basic_stream&> operator<<(float_type value) noexcept {
			ensureSpace(64);
			bufferLength += writeFloat(streamBuffer + bufferLength, value);
			return *this;
		}

		inline basic_stream& operator<<(std::string_view value) noexcept {
			writeRaw(value.data(), value.size());
			return *this;
		}

		inline void writeRaw(const char* data, uint64_t size) noexcept {
			if (size >= bufferSize) {
				doFlush();
				rawWrite(stream_target, data, size);
				return;
			}
			ensureSpace(size);
			jsonifierMemcpy(streamBuffer + bufferLength, data, size);
			bufferLength += size;
		}

		inline basic_stream& operator<<(bool value) noexcept {
			return (*this) << std::string_view(value ? "true" : "false");
		}

		inline basic_stream& operator<<(const char* value) noexcept {
			return (*this) << std::string_view(value);
		}

		inline basic_stream& operator<<(char value) noexcept {
			ensureSpace(1);
			streamBuffer[bufferLength++] = value;
			return *this;
		}

		inline void doFlush() noexcept {
			if (bufferLength > 0) {
				rawWrite(stream_target, streamBuffer, bufferLength);
				bufferLength = 0;
			}
		}

		inline void ensureSpace(uint64_t needed) noexcept {
			if (bufferLength + needed > bufferSize) {
				doFlush();
			}
		}

		inline basic_stream& operator<<(flush_t) noexcept {
			doFlush();
			return *this;
		}

		inline basic_stream& operator<<(endl_t) noexcept {
			(*this) << '\n';
			return *this;
		}

		inline std::string_view view() const noexcept {
			return { streamBuffer, static_cast<size_t>(bufferLength) };
		}

		inline void flushNow() noexcept {
			doFlush();
		}

		inline void discard() noexcept {
			bufferLength = 0;
		}

		inline ~basic_stream() noexcept {
			doFlush();
		}

		uint64_t bufferLength;
		stream_targets stream_target;
		char streamBuffer[bufferSize];
	};

#if JSONIFIER_COMPILER_CLANG
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wexit-time-destructors"
	#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

	inline basic_stream<> cout{ stream_targets::stdout_target };

	inline basic_stream<> err{ stream_targets::stderr_target };

#if JSONIFIER_COMPILER_CLANG
	#pragma clang diagnostic pop
#endif

}
