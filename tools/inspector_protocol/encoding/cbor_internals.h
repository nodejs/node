// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INSPECTOR_PROTOCOL_ENCODING_CBOR_INTERNALS_H_
#define INSPECTOR_PROTOCOL_ENCODING_CBOR_INTERNALS_H_

#include <cstdint>
#include <vector>
#include "span.h"
#include "status.h"

// These internals are exposed for testing and implementing cbor.h.
// Never directly depend on them from other production code.
namespace inspector_protocol {
namespace cbor {
enum class MajorType;
}

namespace cbor_internals {

// Reads the start of a token with definitive size from |bytes|.
// |type| is the major type as specified in RFC 7049 Section 2.1.
// |value| is the payload (e.g. for MajorType::UNSIGNED) or is the size
// (e.g. for BYTE_STRING).
// If successful, returns the number of bytes read. Otherwise returns -1.
int8_t ReadTokenStart(span<uint8_t> bytes, cbor::MajorType* type,
                      uint64_t* value);

// Writes the start of a token with |type|. The |value| may indicate the size,
// or it may be the payload if the value is an unsigned integer.
void WriteTokenStart(cbor::MajorType type, uint64_t value,
                     std::vector<uint8_t>* encoded);
}  // namespace cbor_internals
}  // namespace inspector_protocol

#endif  // INSPECTOR_PROTOCOL_ENCODING_CBOR_H_
