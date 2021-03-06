// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

load('test/mjsunit/test-async.js');

(function() {
  testAsync(assert => {
    assert.plan(1);
    Promise.any([]).then(
      assert.unreachable,
      (x) => { assert.equals(0, x.errors.length); }
    );
  });
})();

(function() {
  const p1 = Promise.resolve(1);
  const p2 = Promise.resolve(2);
  const p3 = Promise.resolve(3);
  testAsync(assert => {
    assert.plan(1);
    Promise.any([p1, p2, p3]).then(
      (x) => { assert.equals(1, x); },
      assert.unreachable);
    });
})();

(function() {
  let outsideResolve;
  let outsideReject;
  let p1 = new Promise(() => {});
  let p2 = new Promise(function(resolve, reject) {
      outsideResolve = resolve;
      outsideReject = reject;
  });
  let p3 = new Promise(() => {});
  testAsync(assert => {
    assert.plan(1);
    Promise.any([p1, p2, p3]).then(
      (x) => { assert.equals(2, x); },
      assert.unreachable
    );
    outsideResolve(2);
    });
})();

(function() {
  const p1 = Promise.reject(1);
  const p2 = Promise.resolve(2);
  const p3 = Promise.resolve(3);
  testAsync(assert => {
    assert.plan(1);
    Promise.any([p1, p2, p3]).then(
      (x) => { assert.equals(2, x); },
      assert.unreachable);
    });
})();

(function() {
  const p1 = Promise.reject(1);
  const p2 = Promise.reject(2);
  const p3 = Promise.reject(3);
  testAsync(assert => {
    assert.plan(4);
    Promise.any([p1, p2, p3]).then(
      assert.unreachable,
      (x) => {
        assert.equals(3, x.errors.length);
        assert.equals(1, x.errors[0]);
        assert.equals(2, x.errors[1]);
        assert.equals(3, x.errors[2]);
      }
    );
  });
})();

(function() {
  testAsync(assert => {
    assert.plan(1);
    (async function() {
      const p1 = Promise.reject(1);
      const p2 = Promise.reject(2);
      const p3 = Promise.reject(3);
      try {
        await Promise.any([p1, p2, p3]);
      } catch (error) {
        assert.equals(1, 1);
      }
    })();
  });
})();

// Test that we return a proper array even if (custom) "then" invokes the
// reject callbacks right away.
(function() {
  class MyPromise extends Promise {
    constructor(executor, id) {
      super(executor);
      this.id = id;
    }

    then(resolve, reject) {
      if (this.id) return reject(this.id);
      return super.then(resolve, reject)
    }
  };
  const a = new MyPromise(() => {}, 'a');
  const b = new MyPromise(() => {}, 'b');
  testAsync(assert => {
    assert.plan(1);
    MyPromise.any([a, b]).then(
      assert.unreachable,
        (e) => { assert.equals(['a', 'b'], e.errors) });
  });
})();

(function TestErrorsProperties() {
  testAsync(assert => {
    assert.plan(3);
    Promise.any([]).catch(
      (error) =>  {
        let desc = Object.getOwnPropertyDescriptor(error, 'errors');
        assert.equals(true, desc.configurable);
        assert.equals(false, desc.enumerable);
        assert.equals(true, desc.writable);
    });
  });
})();
