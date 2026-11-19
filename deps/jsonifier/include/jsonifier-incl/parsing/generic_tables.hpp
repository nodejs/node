/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/parsing/generic_tables.hpp
 */
#pragma once

#include <jsonifier-incl/parsing/generic_common_pre.hpp>

namespace jsonifier::internal {

	// Sampled from Dr. Lemire's library, simdjson: https://github.com/simdjson/simdjson
	consteval internal::array<int8_t, 256> genDepthDeltaTable() noexcept {
		internal::array<int8_t, 256> returnValues{};
		for (auto& entry: returnValues) {
			entry = 0;
		}
		returnValues[uint_c_v<'{'>] = 1;
		returnValues[uint_c_v<'['>] = 1;
		returnValues[uint_c_v<'}'>] = -1;
		returnValues[uint_c_v<']'>] = -1;
		return returnValues;
	}

	alignas(64) inline constexpr auto depthDeltaTableRaw{ genDepthDeltaTable() };

	inline constexpr const int8_t* __restrict depthDeltaTable{ depthDeltaTableRaw.data() };

	using array_type_table = internal::array<json_types, 256>;

	consteval array_type_table genTypeTable() noexcept {
		array_type_table returnValues{};
		for (auto& entry: returnValues) {
			entry = json_types::unset;
		}
		returnValues[uint_c_v<'{'>] = json_types::object;
		returnValues[uint_c_v<'['>] = json_types::array;
		returnValues[uint_c_v<'"'>] = json_types::string;
		returnValues[uint_c_v<'t'>] = json_types::boolean;
		returnValues[uint_c_v<'f'>] = json_types::boolean;
		returnValues[uint_c_v<'n'>] = json_types::null;
		returnValues[uint_c_v<'-'>] = json_types::number;
		for (uint64_t x = uint_c_v<'0'>; x <= uint_c_v<'9'>; ++x) {
			returnValues[x] = json_types::number;
		}
		return returnValues;
	}

	alignas(64) inline constexpr auto typeTableRaw{ genTypeTable() };

	inline constexpr const json_types* __restrict typeTable{ typeTableRaw.data() };

	consteval internal::array<bool, 256> genSeperatorByteTable() noexcept {
		internal::array<bool, 256> returnValues{};
		for (auto& entry: returnValues) {
			entry = false;
		}
		returnValues[uint_c_v<' '>]	 = true;
		returnValues[uint_c_v<'\n'>] = true;
		returnValues[uint_c_v<'\r'>] = true;
		returnValues[uint_c_v<'\t'>] = true;
		returnValues[uint_c_v<','>]	 = true;
		return returnValues;
	}

	alignas(64) inline constexpr auto seperatorByteTableRaw{ genSeperatorByteTable() };

	inline constexpr const bool* __restrict seperatorByteTable{ seperatorByteTableRaw.data() };

	consteval internal::array<bool, 256ULL> genUtf8StringStopTable() noexcept {
		internal::array<bool, 256ULL> returnValues{};
		for (uint64_t x = 0; x < 0x20; ++x) {
			returnValues[x] = true;
		}
		returnValues[uint_c_v<'"'>]	 = true;
		returnValues[uint_c_v<'\\'>] = true;
		return returnValues;
	}

	alignas(64) inline constexpr auto utf8StringStopTableRaw{ genUtf8StringStopTable() };

	inline constexpr const bool* __restrict utf8StringStopTable{ utf8StringStopTableRaw.data() };

	consteval internal::array<bool, 256ULL> genAsciiStringStopTable() noexcept {
		internal::array<bool, 256ULL> returnValues{};
		for (uint64_t x = 0; x < 0x20; ++x) {
			returnValues[x] = true;
		}
		for (uint64_t x = 0x80; x < 0x100; ++x) {
			returnValues[x] = true;
		}
		returnValues[uint_c_v<'"'>]	 = true;
		returnValues[uint_c_v<'\\'>] = true;
		return returnValues;
	}

	alignas(64) inline constexpr auto asciiStringStopTableRaw{ genAsciiStringStopTable() };

	inline constexpr const bool* __restrict asciiStringStopTable{ asciiStringStopTableRaw.data() };

}
