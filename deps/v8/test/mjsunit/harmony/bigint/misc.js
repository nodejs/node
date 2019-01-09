// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Create BigInt in large object space for which MakeImmutable reduces the
// length.
const x = 2n ** (2n ** 22n);
assertEquals(1n, x - (x - 1n));
