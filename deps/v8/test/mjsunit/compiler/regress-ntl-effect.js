// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function g() {
  throw 0;
}

function f() {
  g();
  while (1) {}
}

assertThrows(function () { f(); });
