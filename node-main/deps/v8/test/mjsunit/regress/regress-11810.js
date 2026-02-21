// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test(object, property) {
  assertEquals('pass', object[property]);
}

(function TestStringProperty() {
  let o1 = {get foo() { return 'fail'}, 'foo':'pass'};
  test(o1, 'foo');
  let o2 = {'foo':'fail', get foo() { return 'pass'}, };
  test(o2, 'foo');
})();

(function TestNumberProperty() {
  let o1 = {get 0() { return 'fail'}, 0:'pass'};
  test(o1, 0);
  let o2 = {0:'fail', get 0() { return 'pass'}};
  test(o2, 0);
})();

(function TestBigNumberProperty() {
  // Test a number which is too big to be considered an array index.
  let o1 = {get 50000000000() { return 'fail'}, 50000000000:'pass'};
  test(o1, 50000000000);
  let o2 = {50000000000:'fail', get 50000000000() { return 'pass'}};
  test(o2, 50000000000);
})();
