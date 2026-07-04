// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let v12 = Uint8Array.from(
    [255, 15, 111, 34, 1, 97, 109, 255, 255, 255, 255, 255, 0, 63, 67, 123, 1]);
assertThrows(
    () => d8.serializer.deserialize(v12.buffer), Error,
    'Unable to deserialize cloned data.');
