// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function outer() {
  var x = 1;
  var y = 1;
  function middle(a = eval("var x = 2"), b = () => x + y) {
    var y = 3;
    function inner() {
      return x + y;
    }
    return [b(), inner()];
  }
  return middle();
}
assertEquals([3, 5], outer());

function outerArrow() {
  var x = 10;
  function middle() {
    var f = (a = eval("var x = 20"), b = () => x) => b() + a;
    function inner() {
      return x;
    }
    return [f(5), inner()];
  }
  return middle();
}
assertEquals([15, 10], outerArrow());
