'use strict';
var common = require('../common');
var assert = require('assert');
var domain = require('domain');

var asyncTest = (function() {
  var asyncTestsEnabled = false;
  var asyncTestLastCheck;
  var asyncTestQueue = [];
  var asyncTestHandle;
  var currentTest = null;

  function fail(error) {
    var stack = currentTest
          ? error.stack + '\nFrom previous event:\n' + currentTest.stack
          : error.stack;

    if (currentTest)
      process.stderr.write('\'' + currentTest.description + '\' failed\n\n');

    process.stderr.write(stack);
    process.exit(2);
  }

  function nextAsyncTest() {
    var called = false;
    function done(err) {
      if (called) return fail(new Error('done called twice'));
      called = true;
      asyncTestLastCheck = Date.now();
      if (arguments.length > 0) return fail(err);
      setTimeout(nextAsyncTest, 10);
    }

    if (asyncTestQueue.length) {
      var test = asyncTestQueue.shift();
      currentTest = test;
      test.action(done);
    } else {
      clearInterval(asyncTestHandle);
    }
  }

  return function asyncTest(description, fn) {
    var stack = new Error().stack.split('\n').slice(1).join('\n');
    asyncTestQueue.push({
      action: fn,
      stack: stack,
      description: description
    });
    if (!asyncTestsEnabled) {
      asyncTestsEnabled = true;
      asyncTestLastCheck = Date.now();
      process.on('uncaughtException', fail);
      asyncTestHandle = setInterval(function() {
        var now = Date.now();
        if (now - asyncTestLastCheck > 10000) {
          return fail(new Error('Async test timeout exceeded'));
        }
      }, 10);
      setTimeout(nextAsyncTest, 10);
    }
  };

})();

function setupException(fn) {
  var listeners = process.listeners('uncaughtException');
  process.removeAllListeners('uncaughtException');
  process.on('uncaughtException', fn);
  return function clean() {
    process.removeListener('uncaughtException', fn);
    listeners.forEach(function(listener) {
      process.on('uncaughtException', listener);
    });
  };
}

function clean() {
  process.removeAllListeners('unhandledRejection');
  process.removeAllListeners('rejectionHandled');
}

function onUnhandledSucceed(done, predicate) {
  clean();
  process.on('unhandledRejection', function(reason, promise) {
    try {
      predicate(reason, promise);
    } catch (e) {
      return done(e);
    }
    done();
  });
}

function onUnhandledFail(done) {
  clean();
  process.on('unhandledRejection', function(reason, promise) {
    done(new Error('unhandledRejection not supposed to be triggered'));
  });
  process.on('rejectionHandled', function() {
    done(new Error('rejectionHandled not supposed to be triggered'));
  });
  setTimeout(function() {
    done();
  }, 10);
}

asyncTest('synchronously rejected promise should trigger' +
          ' unhandledRejection', function(done) {
  var e = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(e, reason);
  });
  Promise.reject(e);
});

asyncTest('synchronously rejected promise should trigger' +
          ' unhandledRejection', function(done) {
  var e = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(e, reason);
  });
  new Promise(function(_, reject) {
    reject(e);
  });
});

asyncTest('Promise rejected after setImmediate should trigger' +
          ' unhandledRejection', function(done) {
  var e = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(e, reason);
  });
  new Promise(function(_, reject) {
    setImmediate(function() {
      reject(e);
    });
  });
});

asyncTest('Promise rejected after setTimeout(,1) should trigger' +
          ' unhandled rejection', function(done) {
  var e = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(e, reason);
  });
  new Promise(function(_, reject) {
    setTimeout(function() {
      reject(e);
    }, 1);
  });
});

asyncTest('Catching a promise rejection after setImmediate is not' +
          ' soon enough to stop unhandledRejection', function(done) {
  var e = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(e, reason);
  });
  var _reject;
  var promise = new Promise(function(_, reject) {
    _reject = reject;
  });
  _reject(e);
  setImmediate(function() {
    promise.then(common.fail, function() {});
  });
});

asyncTest('When re-throwing new errors in a promise catch, only the' +
          ' re-thrown error should hit unhandledRejection', function(done) {
  var e = new Error();
  var e2 = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(e2, reason);
    assert.strictEqual(promise2, promise);
  });
  var promise2 = Promise.reject(e).then(common.fail, function(reason) {
    assert.strictEqual(e, reason);
    throw e2;
  });
});

asyncTest('Test params of unhandledRejection for a synchronously-rejected' +
          'promise', function(done) {
  var e = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(e, reason);
    assert.strictEqual(promise, promise);
  });
  Promise.reject(e);
});

asyncTest('When re-throwing new errors in a promise catch, only the ' +
          're-thrown error should hit unhandledRejection: original promise' +
          ' rejected async with setTimeout(,1)', function(done) {
  var e = new Error();
  var e2 = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(e2, reason);
    assert.strictEqual(promise2, promise);
  });
  var promise2 = new Promise(function(_, reject) {
    setTimeout(function() {
      reject(e);
    }, 1);
  }).then(common.fail, function(reason) {
    assert.strictEqual(e, reason);
    throw e2;
  });
});

asyncTest('When re-throwing new errors in a promise catch, only the re-thrown' +
          ' error should hit unhandledRejection: promise catch attached a' +
          ' process.nextTick after rejection', function(done) {
  var e = new Error();
  var e2 = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(e2, reason);
    assert.strictEqual(promise2, promise);
  });
  var promise = new Promise(function(_, reject) {
    setTimeout(function() {
      reject(e);
      process.nextTick(function() {
        promise2 = promise.then(common.fail, function(reason) {
          assert.strictEqual(e, reason);
          throw e2;
        });
      });
    }, 1);
  });
  var promise2;
});

asyncTest(
  'unhandledRejection should not be triggered if a promise catch is' +
  ' attached synchronously upon the promise\'s creation',
  function(done) {
    var e = new Error();
    onUnhandledFail(done);
    Promise.reject(e).then(common.fail, function() {});
  }
);

asyncTest(
  'unhandledRejection should not be triggered if a promise catch is' +
  ' attached synchronously upon the promise\'s creation',
  function(done) {
    var e = new Error();
    onUnhandledFail(done);
    new Promise(function(_, reject) {
      reject(e);
    }).then(common.fail, function() {});
  }
);

asyncTest('Attaching a promise catch in a process.nextTick is soon enough to' +
          ' prevent unhandledRejection', function(done) {
  var e = new Error();
  onUnhandledFail(done);
  var promise = Promise.reject(e);
  process.nextTick(function() {
    promise.then(common.fail, function() {});
  });
});

asyncTest('Attaching a promise catch in a process.nextTick is soon enough to' +
          ' prevent unhandledRejection', function(done) {
  var e = new Error();
  onUnhandledFail(done);
  var promise = new Promise(function(_, reject) {
    reject(e);
  });
  process.nextTick(function() {
    promise.then(common.fail, function() {});
  });
});

asyncTest('While inside setImmediate, catching a rejected promise derived ' +
          'from returning a rejected promise in a fulfillment handler ' +
          'prevents unhandledRejection', function(done) {
  onUnhandledFail(done);

  setImmediate(function() {
    // reproduces on first tick and inside of setImmediate
    Promise
      .resolve('resolve')
      .then(function() {
        return Promise.reject('reject');
      }).catch(function(e) {});
  });
});

// State adapation tests
asyncTest('catching a promise which is asynchronously rejected (via' +
          'resolution to an asynchronously-rejected promise) prevents' +
          ' unhandledRejection', function(done) {
  var e = new Error();
  onUnhandledFail(done);
  Promise.resolve().then(function() {
    return new Promise(function(_, reject) {
      setTimeout(function() {
        reject(e);
      }, 1);
    });
  }).then(common.fail, function(reason) {
    assert.strictEqual(e, reason);
  });
});

asyncTest('Catching a rejected promise derived from throwing in a' +
          ' fulfillment handler prevents unhandledRejection', function(done) {
  var e = new Error();
  onUnhandledFail(done);
  Promise.resolve().then(function() {
    throw e;
  }).then(common.fail, function(reason) {
    assert.strictEqual(e, reason);
  });
});

asyncTest('Catching a rejected promise derived from returning a' +
          ' synchronously-rejected promise in a fulfillment handler' +
          ' prevents unhandledRejection', function(done) {
  var e = new Error();
  onUnhandledFail(done);
  Promise.resolve().then(function() {
    return Promise.reject(e);
  }).then(common.fail, function(reason) {
    assert.strictEqual(e, reason);
  });
});

asyncTest('A rejected promise derived from returning an' +
          ' asynchronously-rejected promise in a fulfillment handler' +
          ' does trigger unhandledRejection', function(done) {
  var e = new Error();
  var _promise;
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(e, reason);
    assert.strictEqual(_promise, promise);
  });
  _promise = Promise.resolve().then(function() {
    return new Promise(function(_, reject) {
      setTimeout(function() {
        reject(e);
      }, 1);
    });
  });
});

asyncTest('A rejected promise derived from throwing in a fulfillment handler' +
          ' does trigger unhandledRejection', function(done) {
  var e = new Error();
  var _promise;
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(e, reason);
    assert.strictEqual(_promise, promise);
  });
  _promise = Promise.resolve().then(function() {
    throw e;
  });
});

asyncTest(
  'A rejected promise derived from returning a synchronously-rejected' +
  ' promise in a fulfillment handler does trigger unhandledRejection',
  function(done) {
    var e = new Error();
    var _promise;
    onUnhandledSucceed(done, function(reason, promise) {
      assert.strictEqual(e, reason);
      assert.strictEqual(_promise, promise);
    });
    _promise = Promise.resolve().then(function() {
      return Promise.reject(e);
    });
  }
);

// Combinations with Promise.all
asyncTest('Catching the Promise.all() of a collection that includes a' +
          'rejected promise prevents unhandledRejection', function(done) {
  var e = new Error();
  onUnhandledFail(done);
  Promise.all([Promise.reject(e)]).then(common.fail, function() {});
});

asyncTest(
  'Catching the Promise.all() of a collection that includes a ' +
  'nextTick-async rejected promise prevents unhandledRejection',
  function(done) {
    var e = new Error();
    onUnhandledFail(done);
    var p = new Promise(function(_, reject) {
      process.nextTick(function() {
        reject(e);
      });
    });
    p = Promise.all([p]);
    process.nextTick(function() {
      p.then(common.fail, function() {});
    });
  }
);

asyncTest('Failing to catch the Promise.all() of a collection that includes' +
          ' a rejected promise triggers unhandledRejection for the returned' +
          ' promise, not the passed promise', function(done) {
  var e = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(e, reason);
    assert.strictEqual(p, promise);
  });
  var p = Promise.all([Promise.reject(e)]);
});

asyncTest('Waiting setTimeout(, 10) to catch a promise causes an' +
          ' unhandledRejection + rejectionHandled pair', function(done) {
  clean();
  var unhandledPromises = [];
  var e = new Error();
  process.on('unhandledRejection', function(reason, promise) {
    assert.strictEqual(e, reason);
    unhandledPromises.push(promise);
  });
  process.on('rejectionHandled', function(promise) {
    assert.strictEqual(1, unhandledPromises.length);
    assert.strictEqual(unhandledPromises[0], promise);
    assert.strictEqual(thePromise, promise);
    done();
  });

  var thePromise = new Promise(function() {
    throw e;
  });
  setTimeout(function() {
    thePromise.then(common.fail, function(reason) {
      assert.strictEqual(e, reason);
    });
  }, 10);
});

asyncTest('Waiting for some combination of process.nextTick + promise' +
          ' microtasks to attach a catch handler is still soon enough to' +
          ' prevent unhandledRejection', function(done) {
  var e = new Error();
  onUnhandledFail(done);


  var a = Promise.reject(e);
  process.nextTick(function() {
    Promise.resolve().then(function() {
      process.nextTick(function() {
        Promise.resolve().then(function() {
          a.catch(function() {});
        });
      });
    });
  });
});

asyncTest('Waiting for some combination of process.nextTick + promise' +
          ' microtasks to attach a catch handler is still soon enough to ' +
          'prevent unhandledRejection: inside setImmediate', function(done) {
  var e = new Error();
  onUnhandledFail(done);

  setImmediate(function() {
    var a = Promise.reject(e);
    process.nextTick(function() {
      Promise.resolve().then(function() {
        process.nextTick(function() {
          Promise.resolve().then(function() {
            a.catch(function() {});
          });
        });
      });
    });
  });
});

asyncTest('Waiting for some combination of process.nextTick + promise ' +
          'microtasks to attach a catch handler is still soon enough to ' +
          'prevent unhandledRejection: inside setTimeout', function(done) {
  var e = new Error();
  onUnhandledFail(done);

  setTimeout(function() {
    var a = Promise.reject(e);
    process.nextTick(function() {
      Promise.resolve().then(function() {
        process.nextTick(function() {
          Promise.resolve().then(function() {
            a.catch(function() {});
          });
        });
      });
    });
  }, 0);
});

asyncTest('Waiting for some combination of promise microtasks + ' +
          'process.nextTick to attach a catch handler is still soon enough' +
          ' to prevent unhandledRejection', function(done) {
  var e = new Error();
  onUnhandledFail(done);


  var a = Promise.reject(e);
  Promise.resolve().then(function() {
    process.nextTick(function() {
      Promise.resolve().then(function() {
        process.nextTick(function() {
          a.catch(function() {});
        });
      });
    });
  });
});

asyncTest(
  'Waiting for some combination of promise microtasks +' +
  ' process.nextTick to attach a catch handler is still soon enough' +
  ' to prevent unhandledRejection: inside setImmediate',
  function(done) {
    var e = new Error();
    onUnhandledFail(done);

    setImmediate(function() {
      var a = Promise.reject(e);
      Promise.resolve().then(function() {
        process.nextTick(function() {
          Promise.resolve().then(function() {
            process.nextTick(function() {
              a.catch(function() {});
            });
          });
        });
      });
    });
  }
);

asyncTest('Waiting for some combination of promise microtasks +' +
          ' process.nextTick to attach a catch handler is still soon enough' +
          ' to prevent unhandledRejection: inside setTimeout', function(done) {
  var e = new Error();
  onUnhandledFail(done);

  setTimeout(function() {
    var a = Promise.reject(e);
    Promise.resolve().then(function() {
      process.nextTick(function() {
        Promise.resolve().then(function() {
          process.nextTick(function() {
            a.catch(function() {});
          });
        });
      });
    });
  }, 0);
});

asyncTest('setImmediate + promise microtasks is too late to attach a catch' +
          ' handler; unhandledRejection will be triggered in that case.' +
          ' (setImmediate before promise creation/rejection)', function(done) {
  var e = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(e, reason);
    assert.strictEqual(p, promise);
  });
  var p = Promise.reject(e);
  setImmediate(function() {
    Promise.resolve().then(function() {
      p.catch(function() {});
    });
  });
});

asyncTest('setImmediate + promise microtasks is too late to attach a catch' +
          ' handler; unhandledRejection will be triggered in that case' +
          ' (setImmediate before promise creation/rejection)', function(done) {
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(undefined, reason);
    assert.strictEqual(p, promise);
  });
  setImmediate(function() {
    Promise.resolve().then(function() {
      Promise.resolve().then(function() {
        Promise.resolve().then(function() {
          Promise.resolve().then(function() {
            p.catch(function() {});
          });
        });
      });
    });
  });
  var p = Promise.reject();
});

asyncTest('setImmediate + promise microtasks is too late to attach a catch' +
          ' handler; unhandledRejection will be triggered in that case' +
          ' (setImmediate after promise creation/rejection)', function(done) {
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(undefined, reason);
    assert.strictEqual(p, promise);
  });
  var p = Promise.reject();
  setImmediate(function() {
    Promise.resolve().then(function() {
      Promise.resolve().then(function() {
        Promise.resolve().then(function() {
          Promise.resolve().then(function() {
            p.catch(function() {});
          });
        });
      });
    });
  });
});

asyncTest(
  'Promise unhandledRejection handler does not interfere with domain' +
  ' error handlers being given exceptions thrown from nextTick.',
  function(done) {
    var d = domain.create();
    var domainReceivedError;
    d.on('error', function(e) {
      domainReceivedError = e;
    });
    d.run(function() {
      var e = new Error('error');
      var domainError = new Error('domain error');
      onUnhandledSucceed(done, function(reason, promise) {
        assert.strictEqual(reason, e);
        assert.strictEqual(domainReceivedError, domainError);
        d.dispose();
      });
      Promise.reject(e);
      process.nextTick(function() {
        throw domainError;
      });
    });
  }
);

asyncTest('nextTick is immediately scheduled when called inside an event' +
          ' handler', function(done) {
  clean();
  var e = new Error('error');
  process.on('unhandledRejection', function(reason, promise) {
    var order = [];
    process.nextTick(function() {
      order.push(1);
    });
    setTimeout(function() {
      order.push(2);
      assert.deepStrictEqual([1, 2], order);
      done();
    }, 1);
  });
  Promise.reject(e);
});

asyncTest('Throwing an error inside a rejectionHandled handler goes to' +
          ' unhandledException, and does not cause .catch() to throw an' +
          'exception', function(done) {
  clean();
  var e = new Error();
  var e2 = new Error();
  var tearDownException = setupException(function(err) {
    assert.equal(e2, err);
    tearDownException();
    done();
  });
  process.on('rejectionHandled', function() {
    throw e2;
  });
  var p = Promise.reject(e);
  setTimeout(function() {
    try {
      p.catch(function() {});
    } catch (e) {
      done(new Error('fail'));
    }
  }, 1);
});
