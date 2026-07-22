// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is testing a regression a slow store IC handler, so force allocation of
// type feedback vectors.
//
// Flags: --no-lazy-feedback-allocation

function crash() { assertTrue(false); }
Object.prototype.__defineSetter__("crashOnSet", crash);

function test() {
  const o = { a: 1 };
  return { ...o, crashOnSet: 42 };
}

// Run once to install the slow IC handler.
test();
// Hit the slow handler.
test();
