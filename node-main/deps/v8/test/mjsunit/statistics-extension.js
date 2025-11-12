// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-statistics

assertEquals(typeof getV8Statistics, 'function');
var result = getV8Statistics();
assertEquals(typeof result, 'object');
for (let key of Object.keys(result)) {
  assertEquals(typeof result[key], 'number');
}
