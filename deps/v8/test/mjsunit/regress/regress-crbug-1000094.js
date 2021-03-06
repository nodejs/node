// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-lazy-source-positions --stress-lazy-source-positions

var f = (( {a: b} = {
    a() {
      return b;
    }
}) => b)()();

// b should get assigned to the inner function a, which then ends up returning
// itself.
assertEquals(f, f());
