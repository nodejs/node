// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/test-async.js');

// We store the index in the hash code field of the Promise.all resolve
// element closures, so make sure we properly handle the cases where this
// magical field turns into a PropertyArray later.
(function() {
  class MyPromise extends Promise {
    then(resolve, reject) {
      this.resolve = resolve;
    }
  };
  const myPromise = new MyPromise(() => {});
  MyPromise.all([myPromise]);
  myPromise.resolve.x = 1;
  myPromise.resolve(1);
})();

// Same test as above, but for PropertyDictionary.
(function() {
  class MyPromise extends Promise {
    then(resolve, reject) {
      this.resolve = resolve;
    }
  };
  const myPromise = new MyPromise(() => {});
  MyPromise.all([myPromise]);
  for (let i = 0; i < 1025; ++i) {
    myPromise.resolve[`x${i}`] = i;
  }
  myPromise.resolve(1);
})();

// Test that we return a proper array even if (custom) "then" invokes the
// resolve callbacks right away.
(function() {
  class MyPromise extends Promise {
    constructor(executor, id) {
      super(executor);
      this.id = id;
    }

    then(resolve, reject) {
      if (this.id) return resolve(this.id);
      return super.then(resolve, reject)
    }
  };
  const a = new MyPromise(() => {}, 'a');
  const b = new MyPromise(() => {}, 'b');
  testAsync(assert => {
    assert.plan(1);
    MyPromise.all([a, b]).then(
        v => assert.equals(['a', 'b'], v),
        assert.unexpectedRejection());
  });
})();

// Test that we properly handle holes introduced into the resulting array
// by resolving some late elements immediately.
(function() {
  class MyPromise extends Promise {
    then(resolve, reject) {
      if (this.immediately) {
        resolve(42);
      } else {
        super.then(resolve, reject);
      }
    }
  };
  const a = new Array(1024);
  a.fill(MyPromise.resolve(1));
  const p = MyPromise.resolve(0);
  p.immediately = true;
  a.push(p);
  testAsync(assert => {
    assert.plan(1);
    MyPromise.all(a).then(
        b => assert.equals(42, b[1024]),
        assert.unexpectedRejection());
  });
})();
