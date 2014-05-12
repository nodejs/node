// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-opt

var x = 0;

function f() {
  const c;
  var c;
  return 0 + x;
}

assertThrows(f);
