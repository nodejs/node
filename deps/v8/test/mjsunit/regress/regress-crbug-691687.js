// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --no-lazy-feedback-allocation --no-lazy --turbo-filter=whatever
// Flags: --invocation-count-for-turbofan=1

function g() { eval() }
with ({}) { }
f = ({x}) => x;
assertEquals(42, f({x: 42}));
assertEquals(42, f({x: 42}));
