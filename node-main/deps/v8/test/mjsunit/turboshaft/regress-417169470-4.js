// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbofan

function opt() {
  const nop = 0;
  const empty = {};
  const a = {p1: 42.42};

  function foo(b) {
    nop;

    a.p4 = 42;
    b.p2 = 42;
    b.p5 = empty;
    a.p6 = 41.414;
  }

  a.p3 = 42;
  a.p4 = 42;

  for (let i = 0; i < 300; i++) {
    foo(a);
  }
}

opt();
opt();
opt();
