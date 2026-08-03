// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The value deserializer should be lenient of malformed or malicious data and
// should throw instead of crash on non-existent shared objects in serialized
// data.

(function SharedObject() {
  // Shared object is 'p', ASCII 112.
  const data = new Uint8Array([255, 15, 112, 0]);
  assertThrows(() => { d8.serializer.deserialize(data.buffer); });
})();
