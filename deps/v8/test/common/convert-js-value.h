// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_COMMON_CONVERT_JS_VALUE_H_
#define V8_TEST_COMMON_CONVERT_JS_VALUE_H_

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "include/v8-context.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-maybe.h"
#include "include/v8-primitive.h"
#include "include/v8-value.h"

namespace v8 {

template <typename T>
struct ConvertJSValue {
  static v8::Maybe<T> Get(v8::Local<v8::Value> value,
                          v8::Local<v8::Context> context);
};

template <>
struct ConvertJSValue<int32_t> {
  static v8::Maybe<int32_t> Get(v8::Local<v8::Value> value,
                                v8::Local<v8::Context> context) {
    return value->Int32Value(context);
  }
};

template <>
struct ConvertJSValue<uint32_t> {
  static v8::Maybe<uint32_t> Get(v8::Local<v8::Value> value,
                                 v8::Local<v8::Context> context) {
    return value->Uint32Value(context);
  }
};

template <>
struct ConvertJSValue<std::nullptr_t> {
  static v8::Maybe<std::nullptr_t> Get(v8::Local<v8::Value> value,
                                       v8::Local<v8::Context> context) {
    return value->IsNull() ? v8::Just(nullptr) : v8::Nothing<std::nullptr_t>();
  }
};

// NaNs and +/-Infinity should be 0, otherwise (modulo 2^64) - 2^63.
// Step 8 - 12 of https://heycam.github.io/webidl/#abstract-opdef-converttoint
// The int64_t and uint64_t implementations below are copied from Blink:
// https://source.chromium.org/chromium/chromium/src/+/master:third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h;l=249?q=doubletointeger&sq=&ss=chromium%2Fchromium%2Fsrc
template <>
struct ConvertJSValue<int64_t> {
  static v8::Maybe<int64_t> Get(v8::Local<v8::Value> value,
                                v8::Local<v8::Context> context) {
    v8::Maybe<double> double_value = value->NumberValue(context);
    if (!double_value.IsJust()) {
      return v8::Nothing<int64_t>();
    }
    double result = double_value.ToChecked();
    if (std::isinf(result) || std::isnan(result)) {
      return v8::Just(int64_t(0));
    }
    result = trunc(result);

    constexpr uint64_t kMaxULL = std::numeric_limits<uint64_t>::max();

    // -2^{64} < fmod_value < 2^{64}.
    double fmod_value = fmod(result, static_cast<double>(kMaxULL));
    if (fmod_value >= 0) {
      if (fmod_value < pow(2, 63)) {
        // 0 <= fmod_value < 2^{63}.
        // 0 <= value < 2^{63}. This cast causes no loss.
        return v8::Just(static_cast<int64_t>(fmod_value));
      } else {
        // 2^{63} <= fmod_value < 2^{64}.
        // 2^{63} <= value < 2^{64}. This cast causes no loss.
        return v8::Just(static_cast<int64_t>(fmod_value - pow(2, 64)));
      }
    }
    // -2^{64} < fmod_value < 0.
    // 0 < fmod_value_uint64 < 2^{64}. This cast causes no loss.
    uint64_t fmod_value_uint64 = static_cast<uint64_t>(-fmod_value);
    // -1 < (kMaxULL - fmod_value_uint64) < 2^{64} - 1.
    // 0 < value < 2^{64}.
    return v8::Just(static_cast<int64_t>(kMaxULL - fmod_value_uint64 + 1));
  }
};

template <>
struct ConvertJSValue<uint64_t> {
  static v8::Maybe<uint64_t> Get(v8::Local<v8::Value> value,
                                 v8::Local<v8::Context> context) {
    v8::Maybe<double> double_value = value->NumberValue(context);
    if (!double_value.IsJust()) {
      return v8::Nothing<uint64_t>();
    }
    double result = double_value.ToChecked();
    if (std::isinf(result) || std::isnan(result)) {
      return v8::Just(uint64_t(0));
    }
    result = trunc(result);

    constexpr uint64_t kMaxULL = std::numeric_limits<uint64_t>::max();

    // -2^{64} < fmod_value < 2^{64}.
    double fmod_value = fmod(result, static_cast<double>(kMaxULL));
    if (fmod_value >= 0) {
      return v8::Just(static_cast<uint64_t>(fmod_value));
    }
    // -2^{64} < fmod_value < 0.
    // 0 < fmod_value_uint64 < 2^{64}. This cast causes no loss.
    uint64_t fmod_value_uint64 = static_cast<uint64_t>(-fmod_value);
    // -1 < (kMaxULL - fmod_value_uint64) < 2^{64} - 1.
    // 0 < value < 2^{64}.
    return v8::Just(static_cast<uint64_t>(kMaxULL - fmod_value_uint64 + 1));
  }
};

template <>
struct ConvertJSValue<v8::BigInt> {
  static v8::Maybe<v8::Local<v8::BigInt>> Get(v8::Local<v8::Value> value,
                                              v8::Local<v8::Context> context) {
    if (value->IsBigInt()) {
      return v8::Just(value.As<v8::BigInt>());
    }
    return v8::Nothing<v8::Local<v8::BigInt>>();
  }
};

template <>
struct ConvertJSValue<float> {
  static v8::Maybe<float> Get(v8::Local<v8::Value> value,
                              v8::Local<v8::Context> context) {
    v8::Maybe<double> val = value->NumberValue(context);
    if (val.IsNothing()) return v8::Nothing<float>();
    return v8::Just(static_cast<float>(val.ToChecked()));
  }
};

template <>
struct ConvertJSValue<double> {
  static v8::Maybe<double> Get(v8::Local<v8::Value> value,
                               v8::Local<v8::Context> context) {
    return value->NumberValue(context);
  }
};

template <>
struct ConvertJSValue<bool> {
  static v8::Maybe<bool> Get(v8::Local<v8::Value> value,
                             v8::Local<v8::Context> context) {
    return v8::Just<bool>(value->BooleanValue(v8::Isolate::GetCurrent()));
  }
};

}  // namespace v8

#endif  // V8_TEST_COMMON_CONVERT_JS_VALUE_H_
