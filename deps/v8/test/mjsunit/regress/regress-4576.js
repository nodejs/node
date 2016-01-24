// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-sloppy --legacy-const

// Should trigger a runtime error, not an early error.
function f() {
  const x;
  var x;
}
assertThrows(f, SyntaxError);
