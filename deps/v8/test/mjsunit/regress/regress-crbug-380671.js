// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --mock-arraybuffer-allocator

var buffer = new ArrayBuffer(0xc0000000);
assertEquals(0xc0000000, buffer.byteLength);
