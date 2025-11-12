// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --verify-heap --expose-gc

var ab = new ArrayBuffer(2);
try { new Int32Array(ab); } catch (e) { }
assertEquals(2, ab.byteLength);
gc();
assertEquals(2, ab.byteLength);
