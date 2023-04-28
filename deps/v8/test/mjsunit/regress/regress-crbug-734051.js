// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function TestHeapNumberLiteral() {
    var data = { a: 0, b: 0 };
    data.a += 0.1;
    assertEquals(0.1, data.a);
    assertEquals(0, data.b);
};
TestHeapNumberLiteral();
TestHeapNumberLiteral();
TestHeapNumberLiteral();
TestHeapNumberLiteral();
TestHeapNumberLiteral();
