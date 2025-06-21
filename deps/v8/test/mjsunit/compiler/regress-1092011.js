// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-turbofan --ignore-unhandled-promises

var __caught = 0;

(function main() {
  function foo(f) {
    const pr = new Promise(function executor() {
      f(function resolvefun() {
        try {
          throw 42;
        } catch (e) {
          __caught++;
        }
      }, function rejectfun() {});
    });
    pr.__proto__ = foo.prototype;
    return pr;
  }
  foo.__proto__ = Promise;
  foo.prototype.then = function thenfun() {};
  new foo();
  foo.prototype = undefined;
  foo.all([foo.resolve()]);
})();

assertEquals(2, __caught);
