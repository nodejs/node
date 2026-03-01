// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --js-sum-precise

const sparseArray1 = [1, , 3];
const otherArray = [,2];
sparseArray1.__proto__ = otherArray;
assertEquals(6, Math.sumPrecise(sparseArray1));

Array.prototype[1] = 2;
const sparseArray2 = [1, , 3];
assertEquals(6, Math.sumPrecise(sparseArray2));
