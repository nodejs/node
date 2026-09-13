// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const re = /(?:[^a][\s\S])*/d;
const pattern = '123456789';
const expected_result = '12345678';
const expected_end = 8;
assertTrue(re.hasIndices);
let match = re.exec(pattern);
assertEquals(expected_result, match[0]);
assertEquals(expected_end, match.indices[0][1]);
// Execute a second time to test JIT
match = re.exec(pattern);
assertEquals(expected_result, match[0]);
assertEquals(expected_end, match.indices[0][1]);
