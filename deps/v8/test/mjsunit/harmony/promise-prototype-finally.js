// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-promise-finally --allow-natives-syntax

var asyncAssertsExpected = 0;

function assertUnreachable() {
  %AbortJS("Unreachable: failure");
}

function assertAsyncRan() {
  ++asyncAssertsExpected;
}

function assertAsync(b, s) {
  if (b) {
    print(s, "succeeded");
  } else {
    %AbortJS(s + " FAILED!");
  }
  --asyncAssertsExpected;
}

function assertEqualsAsync(b, s) {
  if (b === s) {
    print(b, "===", s, "succeeded");
  } else {
    %AbortJS(b + "===" + s + " FAILED!");
  }
  --asyncAssertsExpected;
}

function assertAsyncDone(iteration) {
  var iteration = iteration || 0;
  %EnqueueMicrotask(function() {
    if (asyncAssertsExpected === 0)
      assertAsync(true, "all");
    else if (
      iteration > 10 // Shouldn't take more.
    )
      assertAsync(false, "all... " + asyncAssertsExpected);
    else
      assertAsyncDone(iteration + 1);
  });
}

(function() {
  assertThrows(
    function() {
      Promise.prototype.finally.call(5);
    },
    TypeError
  );
})();

// resolve/finally/then
(function() {
  Promise.resolve(3).finally().then(
    x => {
      assertEqualsAsync(3, x);
    },
    assertUnreachable
  );
  assertAsyncRan();
})();

// reject/finally/then
(function() {
  Promise.reject(3).finally().then(assertUnreachable, x => {
    assertEqualsAsync(3, x);
  });
  assertAsyncRan();
})();

// resolve/finally-return-notcallable/then
(function() {
  Promise.resolve(3).finally(2).then(
    x => {
      assertEqualsAsync(3, x);
    },
    assertUnreachable
  );
  assertAsyncRan();
})();

// reject/finally-return-notcallable/then
(function() {
  Promise.reject(3).finally(2).then(
    assertUnreachable, e => {
      assertEqualsAsync(3, e);
    });
  assertAsyncRan();
})();

// reject/finally/catch
(function() {
  Promise.reject(3).finally().catch(reason => {
    assertEqualsAsync(3, reason);
  });
  assertAsyncRan();
})();

// reject/finally/then/catch
(function() {
  Promise.reject(3).finally().then(assertUnreachable).catch(reason => {
    assertEqualsAsync(3, reason);
  });
  assertAsyncRan();
})();

// resolve/then/finally/then
(function() {
  Promise.resolve(3)
    .then(x => {
      assertEqualsAsync(3, x);
      return x;
    })
    .finally()
    .then(
      x => {
        assertEqualsAsync(3, x);
      },
      assertUnreachable
    );
  assertAsyncRan();
  assertAsyncRan();
})();

// reject/catch/finally/then
(function() {
  Promise.reject(3)
    .catch(x => {
      assertEqualsAsync(3, x);
      return x;
    })
    .finally()
    .then(
      x => {
        assertEqualsAsync(3, x);
      },
      assertUnreachable
    );
  assertAsyncRan();
  assertAsyncRan();
})();

// resolve/finally-throw/then
(function() {
  Promise.resolve(3)
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
      throw 1;
    })
    .then(assertUnreachable, function onRejected(reason) {
      assertEqualsAsync(1, reason);
    });
  assertAsyncRan();
  assertAsyncRan();
})();

// reject/finally-throw/then
(function() {
  Promise.reject(3)
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
      throw 1;
    })
    .then(assertUnreachable, function onRejected(reason) {
      assertEqualsAsync(1, reason);
    });
  assertAsyncRan();
  assertAsyncRan();
})();

// resolve/finally-return/then
(function() {
  Promise.resolve(3)
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
      return 4;
    })
    .then(
      x => {
        assertEqualsAsync(x, 3);
      },
      assertUnreachable
    );
  assertAsyncRan();
  assertAsyncRan();
})();

// reject/finally-return/then
(function() {
  Promise.reject(3)
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
      return 4;
    })
    .then(assertUnreachable, x => {
      assertEqualsAsync(x, 3);
    });
  assertAsyncRan();
  assertAsyncRan();
})();

// reject/catch-throw/finally-throw/then
(function() {
  Promise.reject(3)
    .catch(e => {
      assertEqualsAsync(3, e);
      throw e;
    })
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
      throw 4;
    })
    .then(assertUnreachable, function onRejected(e) {
      assertEqualsAsync(4, e);
    });
  assertAsyncRan();
  assertAsyncRan();
  assertAsyncRan();
})();

// resolve/then-throw/finally-throw/then
(function() {
  Promise.resolve(3)
    .then(e => {
      assertEqualsAsync(3, e);
      throw e;
    })
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
      throw 4;
    })
    .then(assertUnreachable, function onRejected(e) {
      assertEqualsAsync(4, e);
    });
  assertAsyncRan();
  assertAsyncRan();
  assertAsyncRan();
})();

// resolve/finally-return-rejected-promise/then
(function() {
  Promise.resolve(3)
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
      return Promise.reject(4);
    })
    .then(assertUnreachable, e => {
      assertEqualsAsync(4, e);
    });
  assertAsyncRan();
  assertAsyncRan();
})();

// reject/finally-return-rejected-promise/then
(function() {
  Promise.reject(3)
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
      return Promise.reject(4);
    })
    .then(assertUnreachable, e => {
      assertEqualsAsync(4, e);
    });
  assertAsyncRan();
  assertAsyncRan();
})();

// resolve/finally-return-resolved-promise/then
(function() {
  Promise.resolve(3)
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
      return Promise.resolve(4);
    })
    .then(
      x => {
        assertEqualsAsync(3, x);
      },
      assertUnreachable
    );
  assertAsyncRan();
  assertAsyncRan();
})();

// reject/finally-return-resolved-promise/then
(function() {
  Promise.reject(3)
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
      return Promise.resolve(4);
    })
    .then(assertUnreachable, e => {
      assertEqualsAsync(3, e);
    });
  assertAsyncRan();
  assertAsyncRan();
})();

// reject/finally-return-resolved-promise/then
(function() {
  Promise.reject(3)
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
      return Promise.resolve(4);
    })
    .then(assertUnreachable, e => {
      assertEqualsAsync(3, e);
    });
  assertAsyncRan();
  assertAsyncRan();
})();

// resolve/finally-thenable-resolve/then
(function() {
  var thenable = {
    then: function(onResolve, onReject) {
      onResolve(5);
    }
  };

  Promise.resolve(5)
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
      return thenable;
    })
    .then(
      x => {
        assertEqualsAsync(5, x);
      },
      assertUnreachable
    );

  assertAsyncRan();
  assertAsyncRan();
})();

// reject/finally-thenable-resolve/then
(function() {
  var thenable = {
    then: function(onResolve, onReject) {
      onResolve(1);
    }
  };

  Promise.reject(5)
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
      return thenable;
    })
    .then(assertUnreachable, e => {
      assertEqualsAsync(5, e);
    });

  assertAsyncRan();
  assertAsyncRan();
})();

// reject/finally-thenable-reject/then
(function() {
  var thenable = {
    then: function(onResolve, onReject) {
      onReject(1);
    }
  };

  Promise.reject(5)
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
      return thenable;
    })
    .then(assertUnreachable, e => {
      assertEqualsAsync(1, e);
    });

  assertAsyncRan();
  assertAsyncRan();
})();

// resolve/finally-thenable-reject/then
(function() {
  var thenable = {
    then: function(onResolve, onReject) {
      onReject(1);
    }
  };

  Promise.resolve(5)
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
      return thenable;
    })
    .then(assertUnreachable, e => {
      assertEqualsAsync(1, e);
    });

  assertAsyncRan();
  assertAsyncRan();
})();

// resolve/finally/finally/then
(function() {
  Promise.resolve(5)
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
    })
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
    })
    .then(
      x => {
        assertEqualsAsync(5, x);
      },
      assertUnreachable
    );

  assertAsyncRan();
  assertAsyncRan();
  assertAsyncRan();
})();

// resolve/finally-throw/finally/then
(function() {
  Promise.resolve(5)
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
      throw 1;
    })
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
    })
    .then(assertUnreachable, e => {
      assertEqualsAsync(1, e);
    });

  assertAsyncRan();
  assertAsyncRan();
  assertAsyncRan();
})();

// resolve/finally-return-rejected-promise/finally/then
(function() {
  Promise.resolve(5)
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
      return Promise.reject(1);
    })
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
    })
    .then(assertUnreachable, e => {
      assertEqualsAsync(1, e);
    });

  assertAsyncRan();
  assertAsyncRan();
  assertAsyncRan();
})();

// reject/finally/finally/then
(function() {
  Promise.reject(5)
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
    })
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
    })
    .then(assertUnreachable, e => {
      assertEqualsAsync(5, e);
    });

  assertAsyncRan();
  assertAsyncRan();
  assertAsyncRan();
})();

// reject/finally-throw/finally/then
(function() {
  Promise.reject(5)
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
      throw 1;
    })
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
    })
    .then(assertUnreachable, e => {
      assertEqualsAsync(1, e);
    });

  assertAsyncRan();
  assertAsyncRan();
  assertAsyncRan();
})();

// reject/finally-return-rejected-promise/finally/then
(function() {
  Promise.reject(5)
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
      return Promise.reject(1);
    })
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
    })
    .then(assertUnreachable, e => {
      assertEqualsAsync(1, e);
    });

  assertAsyncRan();
  assertAsyncRan();
  assertAsyncRan();
})();

// resolve/finally-deferred-resolve/then
(function() {
  var resolve, reject;
  var deferred = new Promise((x, y) => {
    resolve = x;
    reject = y;
  });
  Promise.resolve(1)
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
      return deferred;
    })
    .then(
      x => {
        assertEqualsAsync(1, x);
      },
      assertUnreachable
    );

  assertAsyncRan();
  assertAsyncRan();

  resolve(5);
})();

// resolve/finally-deferred-reject/then
(function() {
  var resolve, reject;
  var deferred = new Promise((x, y) => {
    resolve = x;
    reject = y;
  });
  Promise.resolve(1)
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
      return deferred;
    })
    .then(assertUnreachable, e => {
      assertEqualsAsync(5, e);
    });

  assertAsyncRan();
  assertAsyncRan();

  reject(5);
})();

// all/finally/then
(function() {
  var resolve, reject;
  var deferred = new Promise((x, y) => {
    resolve = x;
    reject = y;
  });

  Promise.all([deferred])
    .finally(function onFinally() {
      assertEqualsAsync(0, arguments.length);
    })
    .then(
      ([x]) => {
        assertEqualsAsync(1, x);
      },
      assertUnreachable
    );

  assertAsyncRan();
  assertAsyncRan();

  resolve(1);
})();

// race/finally/then
(function() {
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
      assertEqualsAsync(0, arguments.length);
    })
    .then(
      x => {
        assertEqualsAsync(1, x);
      },
      assertUnreachable
    );

  assertAsyncRan();
  assertAsyncRan();

  resolve(1);
})();

// resolve/finally-customthen/then
(function() {
  class MyPromise extends Promise {
      then(onFulfilled, onRejected) {
          assertEqualsAsync(5, onFulfilled);
          assertEqualsAsync(5, onRejected);
          return super.then(onFulfilled, onRejected);
      }
  }

  MyPromise.resolve(3).finally(5);

  assertAsyncRan();
  assertAsyncRan();
})();

// reject/finally-customthen/then
(function() {
  class MyPromise extends Promise {
      then(onFulfilled, onRejected) {
          assertEqualsAsync(5, onFulfilled);
          assertEqualsAsync(5, onRejected);
          return super.then(onFulfilled, onRejected);
      }
  }

  MyPromise.reject(3).finally(5);

  assertAsyncRan();
  assertAsyncRan();
})();

var descriptor = Object.getOwnPropertyDescriptor(Promise.prototype, 'finally');
assertTrue(descriptor.writable);
assertTrue(descriptor.configurable);
assertFalse(descriptor.enumerable);
assertEquals("finally", Promise.prototype.finally.name);
assertEquals(1, Promise.prototype.finally.length);

assertAsyncDone();
