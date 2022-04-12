// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --interrupt-budget-for-feedback-allocation=0 --interrupt-budget=1000

(function() {
Empty = function() {};

var FuncImpl = new Function();
Func = function() {
  if (FuncImpl === undefined) {
    try {
      FuncImpl = new Function();
    } catch (e) {
      throw new Error('');
    }
  }
  return FuncImpl();
};
})();

var object = {};
main = function(unused = true) {
  var func = Func();
  Empty(func & object.a & object.a & object.a & object.a !== 0, '');
  Empty(func & object.a !== 0, '');
};

for (let i = 0; i < 40; i++) {
  main();
}
