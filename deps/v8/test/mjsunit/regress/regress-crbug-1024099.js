// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let len = 0x20000;
let ar = new Int32Array(new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * len));
ar[7] = -13;
ar[0x1673] = -42;
ar[0x1f875] = -153;

ar.sort();

assertEquals(ar[0], -153);
assertEquals(ar[1], -42);
assertEquals(ar[2], -13);
assertEquals(ar[3], 0);
