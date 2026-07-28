// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --js-sum-precise

let s = new Set([1, 2, 3]);
let it = s.values();
s.delete(2);
s.delete(3);
let o = {};
s.add(o);
s.delete(o);
assertDoesNotThrow(() => Math.sumPrecise(it));
assertEquals(Math.sumPrecise(s.values()), 1);
