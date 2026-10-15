// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Connect d8's V8Inspector session (`send()` / `receive()`) and lower the C++
// stack limit so 1000-level C++ recursion in `buildDeepSerializedValue` hits
// the stack overflow check on both native and simulator builds.
// Flags: --enable-inspector --stack-size=100

// Build a 1000-level nested array matching `kMaxProtocolDepth`.
let deepArray = [];
for (let i = 0; i < 1000; i++) {
  // Wrap the array one level deeper on each iteration.
  deepArray = [deepArray];
}

// Capture the CDP response delivered synchronously by `send()`.
let response;
globalThis.receive = function(message) {
  response = message;
};

// Trigger deep serialization of `deepArray`, which overflows the C++ stack
// inside `V8SerializationDuplicateTracker`.
send(JSON.stringify({
  id: 1,
  method: 'Runtime.evaluate',
  params: {
    expression: 'deepArray',
    serializationOptions: {serialization: 'deep'}
  }
}));

// Verify that deep serialization gracefully reports the exception instead of
// aborting in `ToLocalChecked()`.
assertEquals(
    'exception during deep serialization', JSON.parse(response).error.message);
