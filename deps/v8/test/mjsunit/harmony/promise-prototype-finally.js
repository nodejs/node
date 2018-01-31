// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-promise-finally --allow-natives-syntax

assertThrows(() => Promise.prototype.finally.call(5), TypeError);

testAsync(assert => {
  assert.plan(1);

  Promise.resolve(3).finally().then(x => {
    assert.equals(3, x);
  }, assert.unreachable);
}, "resolve/finally/then");

testAsync(assert => {
  assert.plan(1);

  Promise.reject(3).finally().then(assert.unreachable, x => {
    assert.equals(3, x);
  });
}, "reject/finally/then");

testAsync(assert => {
  assert.plan(1);

  Promise.resolve(3).finally(2).then(x => {
    assert.equals(3, x);
  }, assert.unreachable);
}, "resolve/finally-return-notcallable/then");

testAsync(assert => {
  assert.plan(1);

  Promise.reject(3).finally(2).then(assert.unreachable, e => {
    assert.equals(3, e);
  });
}, "reject/finally-return-notcallable/then");

testAsync(assert => {
  assert.plan(1);

  Promise.reject(3).finally().catch(reason => {
    assert.equals(3, reason);
  });
}, "reject/finally/catch");

testAsync(assert => {
  assert.plan(1);

  Promise.reject(3).finally().then(assert.unreachable).catch(reason => {
    assert.equals(3, reason);
  });
}, "reject/finally/then/catch");

testAsync(assert => {
  assert.plan(2);

  Promise.resolve(3)
    .then(x => {
      assert.equals(3, x);
      return x;
    })
    .finally()
    .then(x => {
      assert.equals(3, x);
    }, assert.unreachable);
}, "resolve/then/finally/then");

testAsync(assert => {
  assert.plan(2);

  Promise.reject(3)
    .catch(x => {
      assert.equals(3, x);
      return x;
    })
    .finally()
    .then(x => {
      assert.equals(3, x);
    }, assert.unreachable);
}, "reject/catch/finally/then");

testAsync(assert => {
  assert.plan(2);

  Promise.resolve(3)
    .finally(function onFinally() {
      print("in finally");
      assert.equals(0, arguments.length);
      throw 1;
    })
    .then(assert.unreachable, function onRejected(reason) {
      assert.equals(1, reason);
    });
}, "resolve/finally-throw/then");

testAsync(assert => {
  assert.plan(2);

  Promise.reject(3)
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
      throw 1;
    })
    .then(assert.unreachable, function onRejected(reason) {
      assert.equals(1, reason);
    });
}, "reject/finally-throw/then");

testAsync(assert => {
  assert.plan(2);

  Promise.resolve(3)
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
      return 4;
    })
    .then(x => {
      assert.equals(x, 3);
    }, assert.unreachable);
}, "resolve/finally-return/then");

// reject/finally-return/then
testAsync(assert => {
  assert.plan(2);

  Promise.reject(3)
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
      return 4;
    })
    .then(assert.unreachable, x => {
      assert.equals(x, 3);
    });
});

// reject/catch-throw/finally-throw/then
testAsync(assert => {
  assert.plan(3);

  Promise.reject(3)
    .catch(e => {
      assert.equals(3, e);
      throw e;
    })
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
      throw 4;
    })
    .then(assert.unreachable, function onRejected(e) {
      assert.equals(4, e);
    });
});

testAsync(assert => {
  assert.plan(3);

  Promise.resolve(3)
    .then(e => {
      assert.equals(3, e);
      throw e;
    })
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
      throw 4;
    })
    .then(assert.unreachable, function onRejected(e) {
      assert.equals(4, e);
    });
}, "resolve/then-throw/finally-throw/then");

testAsync(assert => {
  assert.plan(2);

  Promise.resolve(3)
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
      return Promise.reject(4);
    })
    .then(assert.unreachable, e => {
      assert.equals(4, e);
    });
}, "resolve/finally-return-rejected-promise/then");

testAsync(assert => {
  assert.plan(2);

  Promise.reject(3)
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
      return Promise.reject(4);
    })
    .then(assert.unreachable, e => {
      assert.equals(4, e);
    });
}, "reject/finally-return-rejected-promise/then");

testAsync(assert => {
  assert.plan(2);

  Promise.resolve(3)
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
      return Promise.resolve(4);
    })
    .then(x => {
      assert.equals(3, x);
    }, assert.unreachable);
}, "resolve/finally-return-resolved-promise/then");

testAsync(assert => {
  assert.plan(2);

  Promise.reject(3)
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
      return Promise.resolve(4);
    })
    .then(assert.unreachable, e => {
      assert.equals(3, e);
    });
}, "reject/finally-return-resolved-promise/then");

testAsync(assert => {
  assert.plan(2);

  Promise.reject(3)
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
      return Promise.resolve(4);
    })
    .then(assert.unreachable, e => {
      assert.equals(3, e);
    });
}, "reject/finally-return-resolved-promise/then");

testAsync(assert => {
  assert.plan(2);

  var thenable = {
    then: function(onResolve, onReject) {
      onResolve(5);
    }
  };

  Promise.resolve(5)
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
      return thenable;
    })
    .then(x => {
      assert.equals(5, x);
    }, assert.unreachable);
}, "resolve/finally-thenable-resolve/then");

testAsync(assert => {
  assert.plan(2);

  var thenable = {
    then: function(onResolve, onReject) {
      onResolve(1);
    }
  };

  Promise.reject(5)
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
      return thenable;
    })
    .then(assert.unreachable, e => {
      assert.equals(5, e);
    });
}, "reject/finally-thenable-resolve/then");

testAsync(assert => {
  assert.plan(2);

  var thenable = {
    then: function(onResolve, onReject) {
      onReject(1);
    }
  };

  Promise.reject(5)
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
      return thenable;
    })
    .then(assert.unreachable, e => {
      assert.equals(1, e);
    });
}, "reject/finally-thenable-reject/then");

testAsync(assert => {
  assert.plan(2);

  var thenable = {
    then: function(onResolve, onReject) {
      onReject(1);
    }
  };

  Promise.resolve(5)
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
      return thenable;
    })
    .then(assert.unreachable, e => {
      assert.equals(1, e);
    });
}, "resolve/finally-thenable-reject/then");

testAsync(assert => {
  assert.plan(3);

  Promise.resolve(5)
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
    })
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
    })
    .then(x => {
      assert.equals(5, x);
    }, assert.unreachable);
}, "resolve/finally/finally/then");

testAsync(assert => {
  assert.plan(3);

  Promise.resolve(5)
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
      throw 1;
    })
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
    })
    .then(assert.unreachable, e => {
      assert.equals(1, e);
    });
}, "resolve/finally-throw/finally/then");

testAsync(assert => {
  assert.plan(3);

  Promise.resolve(5)
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
      return Promise.reject(1);
    })
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
    })
    .then(assert.unreachable, e => {
      assert.equals(1, e);
    });
}, "resolve/finally-return-rejected-promise/finally/then");

testAsync(assert => {
  assert.plan(3);

  Promise.reject(5)
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
    })
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
    })
    .then(assert.unreachable, e => {
      assert.equals(5, e);
    });
}, "reject/finally/finally/then");

testAsync(assert => {
  assert.plan(3);

  Promise.reject(5)
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
      throw 1;
    })
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
    })
    .then(assert.unreachable, e => {
      assert.equals(1, e);
    });
}, "reject/finally-throw/finally/then");

testAsync(assert => {
  assert.plan(3);

  Promise.reject(5)
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
      return Promise.reject(1);
    })
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
    })
    .then(assert.unreachable, e => {
      assert.equals(1, e);
    });
}, "reject/finally-return-rejected-promise/finally/then");

testAsync(assert => {
  assert.plan(2);

  var resolve, reject;
  var deferred = new Promise((x, y) => {
    resolve = x;
    reject = y;
  });

  Promise.resolve(1)
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
      return deferred;
    })
    .then(x => {
      assert.equals(1, x);
    }, assert.unreachable);

  resolve(5);
}, "resolve/finally-deferred-resolve/then");

//
testAsync(assert => {
  assert.plan(2);

  var resolve, reject;
  var deferred = new Promise((x, y) => {
    resolve = x;
    reject = y;
  });
  Promise.resolve(1)
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
      return deferred;
    })
    .then(assert.unreachable, e => {
      assert.equals(5, e);
    });

  reject(5);
}, "resolve/finally-deferred-reject/then");

testAsync(assert => {
  assert.plan(2);

  var resolve, reject;
  var deferred = new Promise((x, y) => {
    resolve = x;
    reject = y;
  });
  Promise.all([deferred])
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
    })
    .then(([x]) => {
      assert.equals(1, x);
    }, assert.unreachable);

  resolve(1);
}, "all/finally/then");

testAsync(assert => {
  assert.plan(2);

  var resolve, reject;
  var d1 = new Promise((x, y) => {
    resolve = x;
    reject = y;
  });
  var d2 = new Promise((x, y) => {
    resolve = x;
    reject = y;
  });
  Promise.race([d1, d2])
    .finally(function onFinally() {
      assert.equals(0, arguments.length);
    })
    .then(x => {
      assert.equals(1, x);
    }, assert.unreachable);

  resolve(1);
}, "race/finally/then");

testAsync(assert => {
  assert.plan(2);

  class MyPromise extends Promise {
    then(onFulfilled, onRejected) {
      assert.equals(5, onFulfilled);
      assert.equals(5, onRejected);
      return super.then(onFulfilled, onRejected);
    }
  }

  MyPromise.resolve(3).finally(5);
}, "resolve/finally-customthen/then");

testAsync(assert => {
  assert.plan(2);

  class MyPromise extends Promise {
    then(onFulfilled, onRejected) {
      assert.equals(5, onFulfilled);
      assert.equals(5, onRejected);
      return super.then(onFulfilled, onRejected);
    }
  }

  MyPromise.reject(3).finally(5);
}, "reject/finally-customthen/then");

var descriptor = Object.getOwnPropertyDescriptor(Promise.prototype, "finally");
assertTrue(descriptor.writable);
assertTrue(descriptor.configurable);
assertFalse(descriptor.enumerable);
assertEquals("finally", Promise.prototype.finally.name);
assertEquals(1, Promise.prototype.finally.length);

var count = 0;
class FooPromise extends Promise {
  constructor(resolve, reject) {
    count++;
    return super(resolve, reject);
  }
}

testAsync(assert => {
  assert.plan(1);
  count = 0;

  new FooPromise(r => r()).finally(() => {}).then(() => {
    assert.equals(6, count);
  });
}, "finally/speciesconstructor");

testAsync(assert => {
  assert.plan(1);
  count = 0;

  FooPromise.resolve().finally(() => {}).then(() => {
    assert.equals(6, count);
  })
}, "resolve/finally/speciesconstructor");

testAsync(assert => {
  assert.plan(1);
  count = 0;

  FooPromise.reject().finally(() => {}).catch(() => {
    assert.equals(6, count);
  })
}, "reject/finally/speciesconstructor");

testAsync(assert => {
  assert.plan(2);

  class MyPromise extends Promise {
    static get [Symbol.species]() { return Promise; }
  }

  var p = Promise
      .resolve()
      .finally(() => MyPromise.resolve());

  assert.equals(true, p instanceof Promise);
  assert.equals(false, p instanceof MyPromise);
}, "finally/Symbol.Species");

testAsync(assert => {
  assert.plan(3);
  let resolve;
  let value = 0;

  let p = new Promise(r => { resolve = r });

  Promise.resolve()
    .finally(() => {
      return p;
    })
    .then(() => {
      value = 1;
    });

  // This makes sure we take the fast path in PromiseResolve that just
  // returns the promise it receives as value. If we had to create
  // another wrapper promise, that would cause an additional tick in
  // the microtask queue.
  Promise.resolve()
    // onFinally has run.
    .then(() => { resolve(); })
    // thenFinally has run.
    .then(() => assert.equals(0, value))
    // promise returned by .finally has been resolved.
    .then(() => assert.equals(0, value))
    // onFulfilled callback of .then() has run.
    .then(() => assert.equals(1, value));

}, "PromiseResolve-ordering");
