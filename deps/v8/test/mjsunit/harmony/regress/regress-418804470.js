// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-base-64

const shared_array_buffer = new SharedArrayBuffer(1.5);
const uint8_array = new Uint8Array(shared_array_buffer);
const from_hex_result = uint8_array.setFromHex('666f6f626172');
const from_base64_result = uint8_array.setFromBase64('Zm9vYmFy');
