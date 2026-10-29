// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function testEager() {
  (function({name = [foo] = eval("[]")}) {})({});
  (function([name = [foo] = eval("[]")]) {})([]);
})();

(function testLazy() {
  function f({name = [foo] = eval("[]")}) {}
  function g([name = [foo] = eval("[]")]) {}
  f({});
  g([]);
})();

(function testEagerArrow() {
  (({name = [foo] = eval("[]")}) => {})({});
  (([name = [foo] = eval("[]")]) => {})([]);
})();

(function testLazyArrow() {
  var f = ({name = [foo] = eval("[]")}) => {};
  var g = ([name = [foo] = eval("[]")]) => {};
  f({});
  g([]);
})();
