// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Should not crash in ASAN.
function shouldThrow() {
    shouldThrow(JSON.parse('{"0":1}'));
}
assertThrows("shouldThrow()", RangeError);
