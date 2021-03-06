// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

load('test/mjsunit/test-async.js');

(function() {
  const p1 = Promise.reject(1);
  const p2 = Promise.resolve(1);
  Object.defineProperty(p2, "then", {});

  testAsync(assert => {
    assert.plan(1);
    Promise.any([p1, p2]).then(
      assert.unreachable,
      (e) => { assert.equals(true, e instanceof TypeError); });
    });
})();
