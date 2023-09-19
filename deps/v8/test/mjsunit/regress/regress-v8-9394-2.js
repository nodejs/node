// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --enable-lazy-source-positions --stress-lazy-source-positions

function test() {
  function f() {
    with ({}) {
      // This is a non-assigning shadowing access to value. If both f and test
      // are fully parsed or both are preparsed, then this is resolved during
      // scope analysis to the outer value, and the outer value knows it can be
      // shadowed. If test is fully parsed and f is preparsed, value here
      // doesn't resolve to anything during partial analysis, and the outer
      // value does not know it can be shadowed.
      return value;
    }
  }
  var value = 2;
  var status = f();
  return value;
}
test();
