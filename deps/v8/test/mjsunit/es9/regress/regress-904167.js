// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Previously, spreading in-object properties would always treat double fields
// as tagged, potentially dereferencing a Float64.

// Ensure that we don't fail an assert from --verify-heap when cloning a
// HeapNumber in the CloneObjectIC handler case.
var src, clone;
for (var i = 0; i < 40000; i++) {
    src = { ...i, x: -9007199254740991 };
    clone = { ...src };
}
