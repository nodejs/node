// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function* g1() {
  try {
    throw {};
  } catch ({a = class extends (yield) {}}) {
  }
}
g1().next();  // crashes without fix

function* g2() {
  let x = function(){};
  try {
    throw {};
  } catch ({b = class extends x {}}) {
  }
}
g2().next();  // crashes without fix

function* g3() {
  let x = 42;
  try {
    throw {};
  } catch ({c = (function() { return x })()}) {
  }
}
g3().next();  // throws a ReferenceError without fix
