// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --stack-size=100

function f(x) {
  new x.Uint16Array();
  function h(y) { /[\cA]/; }
}
let i = 0;
function g() {
  try { g(); } catch (e) {}
  if (i++ > 200) return;  // The original error was at i == 116.
  f();
}
f(this);
g();
