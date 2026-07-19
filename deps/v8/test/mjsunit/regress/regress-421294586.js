// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --force-slow-path

const buffer = new Float16Array(2);
const arr = new Int32Array(buffer.buffer);
arr[0] = 0x7F800001;
assertEquals(0x7F800001, (new Int32Array(buffer.slice().buffer))[0]);
