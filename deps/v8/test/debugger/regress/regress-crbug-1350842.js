// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

debug.Debug.setBreakPoint(f, 2);

function f() {
  function id(x) { return x; } // Force context allocation of `f`.
  new class tmp {
    #foo = id(42);
  };
}
f();
