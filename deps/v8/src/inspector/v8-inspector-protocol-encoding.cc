// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-inspector-protocol-encoding.h"

#include <cmath>
#include "../../third_party/inspector_protocol/encoding/encoding.h"
#include "src/numbers/conversions.h"
#include "src/utils/vector.h"

namespace v8_inspector {
namespace {
using IPEStatus = ::v8_inspector_protocol_encoding::Status;
using ::v8_inspector_protocol_encoding::span;

class Platform : public ::v8_inspector_protocol_encoding::json::Platform {
 public:
  bool StrToD(const char* str, double* result) const override {
    *result = v8::internal::StringToDouble(str, v8::internal::NO_FLAGS);
    return !std::isnan(*result);
  }
  std::unique_ptr<char[]> DToStr(double value) const override {
    v8::internal::ScopedVector<char> buffer(
        v8::internal::kDoubleToCStringMinBufferSize);
    const char* str = v8::internal::DoubleToCString(value, buffer);
    if (str == nullptr) return nullptr;
    std::unique_ptr<char[]> result(new char[strlen(str) + 1]);
    memcpy(result.get(), str, strlen(str) + 1);
    DCHECK_EQ(0, result[strlen(str)]);
    return result;
  }
};
}  // namespace

IPEStatus ConvertCBORToJSON(span<uint8_t> cbor, std::vector<uint8_t>* json) {
  Platform platform;
  return ConvertCBORToJSON(platform, cbor, json);
}

IPEStatus ConvertJSONToCBOR(span<uint8_t> json, std::vector<uint8_t>* cbor) {
  Platform platform;
  return ConvertJSONToCBOR(platform, json, cbor);
}

IPEStatus ConvertJSONToCBOR(span<uint16_t> json, std::vector<uint8_t>* cbor) {
  Platform platform;
  return ConvertJSONToCBOR(platform, json, cbor);
}

}  // namespace v8_inspector
