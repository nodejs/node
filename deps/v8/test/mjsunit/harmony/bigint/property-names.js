// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var { 9007199254740991n: it } = { 9007199254740991n: 1 };
assertEquals(it, 1);

var { 999999999999999999n: it } = { 999999999999999999n: 1 }; // greater than max safe integer
assertEquals(it, 1);
