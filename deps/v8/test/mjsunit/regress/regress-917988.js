// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests if class declarations in parameter list are correctly handled.
function v_2(
v_3 = class v_4 {
    get [[] = ';']() { }
}
) { }
v_2();

// Test object inside a class in a parameter list
(function f(
v_3 = class v_4 {
    get [{} = ';']() { }
}
) { })();

// Test destructuring of class in parameters
(function f( {p, q} = class C { get [[] = ';']() {} } ) {})();

// Test array destructuring of class in parameters
class C {};
C[Symbol.iterator] = function() {
  return {
    next: function() { return { done: true }; },
    _first: true
  };
};
(function f1([p, q] = class D extends C { get [[]]() {} }) { })();
