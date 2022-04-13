// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --always-opt --no-lazy --turbo-filter=whatever

function g() { eval() }
with ({}) { }
f = ({x}) => x;
assertEquals(42, f({x: 42}));
