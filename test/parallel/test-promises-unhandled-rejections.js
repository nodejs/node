'use strict';
const common = require('../common');
const assert = require('assert');
const domain = require('domain');
const { inspect } = require('util');

common.disableCrashOnUnhandledRejection();

const asyncTest = (function() {
  let asyncTestsEnabled = false;
  let asyncTestLastCheck;
  const asyncTestQueue = [];
  let asyncTestHandle;
  let currentTest = null;

  function fail(error) {
    const stack = currentTest ?
      `${inspect(error)}\nFrom previous event:\n${currentTest.stack}` :
      inspect(error);

    if (currentTest)
      process.stderr.write(`'${currentTest.description}' failed\n\n`);

    process.stderr.write(stack);
    process.exit(2);
  }

  function nextAsyncTest() {
    let called = false;
    function done(err) {
      if (called) return fail(new Error('done called twice'));
      called = true;
      asyncTestLastCheck = Date.now();
      if (arguments.length > 0) return fail(err);
      setTimeout(nextAsyncTest, 10);
    }

    if (asyncTestQueue.length) {
      const test = asyncTestQueue.shift();
      currentTest = test;
      test.action(done);
    } else {
      clearInterval(asyncTestHandle);
    }
  }

  return function asyncTest(description, fn) {
    const stack = inspect(new Error()).split('\n').slice(1).join('\n');
    asyncTestQueue.push({
      action: fn,
      stack,
      description
    });
    if (!asyncTestsEnabled) {
      asyncTestsEnabled = true;
      asyncTestLastCheck = Date.now();
      process.on('uncaughtException', fail);
      asyncTestHandle = setInterval(function() {
        const now = Date.now();
        if (now - asyncTestLastCheck > 10000) {
          return fail(new Error('Async test timeout exceeded'));
        }
      }, 10);
      setTimeout(nextAsyncTest, 10);
    }
  };

})();

function setupException(fn) {
  const listeners = process.listeners('uncaughtException');
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
  const e = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(reason, e);
  });
  Promise.reject(e);
});

asyncTest('synchronously rejected promise should trigger' +
          ' unhandledRejection', function(done) {
  const e = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(reason, e);
  });
  new Promise(function(_, reject) {
    reject(e);
  });
});

asyncTest('Promise rejected after setImmediate should trigger' +
          ' unhandledRejection', function(done) {
  const e = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(reason, e);
  });
  new Promise(function(_, reject) {
    setImmediate(function() {
      reject(e);
    });
  });
});

asyncTest('Promise rejected after setTimeout(,1) should trigger' +
          ' unhandled rejection', function(done) {
  const e = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(reason, e);
  });
  new Promise(function(_, reject) {
    setTimeout(function() {
      reject(e);
    }, 1);
  });
});

asyncTest('Catching a promise rejection after setImmediate is not' +
          ' soon enough to stop unhandledRejection', function(done) {
  const e = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(reason, e);
  });
  let _reject;
  const promise = new Promise(function(_, reject) {
    _reject = reject;
  });
  _reject(e);
  setImmediate(function() {
    promise.then(assert.fail, function() {});
  });
});

asyncTest('When re-throwing new errors in a promise catch, only the' +
          ' re-thrown error should hit unhandledRejection', function(done) {
  const e = new Error();
  const e2 = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(reason, e2);
    assert.strictEqual(promise, promise2);
  });
  const promise2 = Promise.reject(e).then(assert.fail, function(reason) {
    assert.strictEqual(reason, e);
    throw e2;
  });
});

asyncTest('Test params of unhandledRejection for a synchronously-rejected ' +
          'promise', function(done) {
  const e = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(reason, e);
    assert.strictEqual(promise, promise);
  });
  Promise.reject(e);
});

asyncTest('When re-throwing new errors in a promise catch, only the ' +
          're-thrown error should hit unhandledRejection: original promise' +
          ' rejected async with setTimeout(,1)', function(done) {
  const e = new Error();
  const e2 = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(reason, e2);
    assert.strictEqual(promise, promise2);
  });
  const promise2 = new Promise(function(_, reject) {
    setTimeout(function() {
      reject(e);
    }, 1);
  }).then(assert.fail, function(reason) {
    assert.strictEqual(reason, e);
    throw e2;
  });
});

asyncTest('When re-throwing new errors in a promise catch, only the re-thrown' +
          ' error should hit unhandledRejection: promise catch attached a' +
          ' process.nextTick after rejection', function(done) {
  const e = new Error();
  const e2 = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(reason, e2);
    assert.strictEqual(promise, promise2);
  });
  const promise = new Promise(function(_, reject) {
    setTimeout(function() {
      reject(e);
      process.nextTick(function() {
        promise2 = promise.then(assert.fail, function(reason) {
          assert.strictEqual(reason, e);
          throw e2;
        });
      });
    }, 1);
  });
  let promise2;
});

asyncTest(
  'unhandledRejection should not be triggered if a promise catch is' +
  ' attached synchronously upon the promise\'s creation',
  function(done) {
    const e = new Error();
    onUnhandledFail(done);
    Promise.reject(e).then(assert.fail, function() {});
  }
);

asyncTest(
  'unhandledRejection should not be triggered if a promise catch is' +
  ' attached synchronously upon the promise\'s creation',
  function(done) {
    const e = new Error();
    onUnhandledFail(done);
    new Promise(function(_, reject) {
      reject(e);
    }).then(assert.fail, function() {});
  }
);

asyncTest('Attaching a promise catch in a process.nextTick is soon enough to' +
          ' prevent unhandledRejection', function(done) {
  const e = new Error();
  onUnhandledFail(done);
  const promise = Promise.reject(e);
  process.nextTick(function() {
    promise.then(assert.fail, function() {});
  });
});

asyncTest('Attaching a promise catch in a process.nextTick is soon enough to' +
          ' prevent unhandledRejection', function(done) {
  const e = new Error();
  onUnhandledFail(done);
  const promise = new Promise(function(_, reject) {
    reject(e);
  });
  process.nextTick(function() {
    promise.then(assert.fail, function() {});
  });
});

asyncTest('While inside setImmediate, catching a rejected promise derived ' +
          'from returning a rejected promise in a fulfillment handler ' +
          'prevents unhandledRejection', function(done) {
  onUnhandledFail(done);

  setImmediate(function() {
    // Reproduces on first tick and inside of setImmediate
    Promise
      .resolve('resolve')
      .then(function() {
        return Promise.reject('reject');
      }).catch(function(e) {});
  });
});

// State adaptation tests
asyncTest('catching a promise which is asynchronously rejected (via ' +
          'resolution to an asynchronously-rejected promise) prevents' +
          ' unhandledRejection', function(done) {
  const e = new Error();
  onUnhandledFail(done);
  Promise.resolve().then(function() {
    return new Promise(function(_, reject) {
      setTimeout(function() {
        reject(e);
      }, 1);
    });
  }).then(assert.fail, function(reason) {
    assert.strictEqual(reason, e);
  });
});

asyncTest('Catching a rejected promise derived from throwing in a' +
          ' fulfillment handler prevents unhandledRejection', function(done) {
  const e = new Error();
  onUnhandledFail(done);
  Promise.resolve().then(function() {
    throw e;
  }).then(assert.fail, function(reason) {
    assert.strictEqual(reason, e);
  });
});

asyncTest('Catching a rejected promise derived from returning a' +
          ' synchronously-rejected promise in a fulfillment handler' +
          ' prevents unhandledRejection', function(done) {
  const e = new Error();
  onUnhandledFail(done);
  Promise.resolve().then(function() {
    return Promise.reject(e);
  }).then(assert.fail, function(reason) {
    assert.strictEqual(reason, e);
  });
});

asyncTest('A rejected promise derived from returning an' +
          ' asynchronously-rejected promise in a fulfillment handler' +
          ' does trigger unhandledRejection', function(done) {
  const e = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(reason, e);
    assert.strictEqual(promise, _promise);
  });
  const _promise = Promise.resolve().then(function() {
    return new Promise(function(_, reject) {
      setTimeout(function() {
        reject(e);
      }, 1);
    });
  });
});

asyncTest('A rejected promise derived from throwing in a fulfillment handler' +
          ' does trigger unhandledRejection', function(done) {
  const e = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(reason, e);
    assert.strictEqual(promise, _promise);
  });
  const _promise = Promise.resolve().then(function() {
    throw e;
  });
});

asyncTest(
  'A rejected promise derived from returning a synchronously-rejected' +
  ' promise in a fulfillment handler does trigger unhandledRejection',
  function(done) {
    const e = new Error();
    onUnhandledSucceed(done, function(reason, promise) {
      assert.strictEqual(reason, e);
      assert.strictEqual(promise, _promise);
    });
    const _promise = Promise.resolve().then(function() {
      return Promise.reject(e);
    });
  }
);

// Combinations with Promise.all
asyncTest('Catching the Promise.all() of a collection that includes a ' +
          'rejected promise prevents unhandledRejection', function(done) {
  const e = new Error();
  onUnhandledFail(done);
  Promise.all([Promise.reject(e)]).then(assert.fail, function() {});
});

asyncTest(
  'Catching the Promise.all() of a collection that includes a ' +
  'nextTick-async rejected promise prevents unhandledRejection',
  function(done) {
    const e = new Error();
    onUnhandledFail(done);
    let p = new Promise(function(_, reject) {
      process.nextTick(function() {
        reject(e);
      });
    });
    p = Promise.all([p]);
    process.nextTick(function() {
      p.then(assert.fail, function() {});
    });
  }
);

asyncTest('Failing to catch the Promise.all() of a collection that includes' +
          ' a rejected promise triggers unhandledRejection for the returned' +
          ' promise, not the passed promise', function(done) {
  const e = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(reason, e);
    assert.strictEqual(promise, p);
  });
  const p = Promise.all([Promise.reject(e)]);
});

asyncTest('Waiting setTimeout(, 10) to catch a promise causes an' +
          ' unhandledRejection + rejectionHandled pair', function(done) {
  clean();
  const unhandledPromises = [];
  const e = new Error();
  process.on('unhandledRejection', function(reason, promise) {
    assert.strictEqual(reason, e);
    unhandledPromises.push(promise);
  });
  process.on('rejectionHandled', function(promise) {
    assert.strictEqual(unhandledPromises.length, 1);
    assert.strictEqual(unhandledPromises[0], promise);
    assert.strictEqual(promise, thePromise);
    done();
  });

  const thePromise = new Promise(function() {
    throw e;
  });
  setTimeout(function() {
    thePromise.then(assert.fail, function(reason) {
      assert.strictEqual(reason, e);
    });
  }, 10);
});

asyncTest('Waiting for some combination of process.nextTick + promise' +
          ' microtasks to attach a catch handler is still soon enough to' +
          ' prevent unhandledRejection', function(done) {
  const e = new Error();
  onUnhandledFail(done);


  const a = Promise.reject(e);
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
  const e = new Error();
  onUnhandledFail(done);

  setImmediate(function() {
    const a = Promise.reject(e);
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
  const e = new Error();
  onUnhandledFail(done);

  setTimeout(function() {
    const a = Promise.reject(e);
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
  const e = new Error();
  onUnhandledFail(done);


  const a = Promise.reject(e);
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
    const e = new Error();
    onUnhandledFail(done);

    setImmediate(function() {
      const a = Promise.reject(e);
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
  const e = new Error();
  onUnhandledFail(done);

  setTimeout(function() {
    const a = Promise.reject(e);
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
  const e = new Error();
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(reason, e);
    assert.strictEqual(promise, p);
  });
  const p = Promise.reject(e);
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
    assert.strictEqual(reason, undefined);
    assert.strictEqual(promise, p);
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
  const p = Promise.reject();
});

asyncTest('setImmediate + promise microtasks is too late to attach a catch' +
          ' handler; unhandledRejection will be triggered in that case' +
          ' (setImmediate after promise creation/rejection)', function(done) {
  onUnhandledSucceed(done, function(reason, promise) {
    assert.strictEqual(reason, undefined);
    assert.strictEqual(promise, p);
  });
  const p = Promise.reject();
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
    const d = domain.create();
    let domainReceivedError;
    d.on('error', function(e) {
      domainReceivedError = e;
    });
    d.run(function() {
      const e = new Error('error');
      const domainError = new Error('domain error');
      onUnhandledSucceed(done, function(reason, promise) {
        assert.strictEqual(reason, e);
        assert.strictEqual(domainReceivedError, domainError);
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
  const e = new Error('error');
  process.on('unhandledRejection', function(reason, promise) {
    const order = [];
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
          ' unhandledException, and does not cause .catch() to throw an ' +
          'exception', function(done) {
  clean();
  const e = new Error();
  const e2 = new Error();
  const tearDownException = setupException(function(err) {
    assert.strictEqual(err, e2);
    tearDownException();
    done();
  });
  process.on('rejectionHandled', function() {
    throw e2;
  });
  const p = Promise.reject(e);
  setTimeout(function() {
    try {
      p.catch(function() {});
    } catch {
      done(new Error('fail'));
    }
  }, 1);
});

asyncTest('Rejected promise inside unhandledRejection allows nextTick loop' +
          ' to proceed first', function(done) {
  clean();
  Promise.reject(0);
  let didCall = false;
  process.on('unhandledRejection', () => {
    assert(!didCall);
    didCall = true;
    const promise = Promise.reject(0);
    process.nextTick(() => promise.catch(() => done()));
  });
});

asyncTest(
  'Unhandled promise rejection emits a warning immediately',
  function(done) {
    clean();
    Promise.reject(0);
    const { emitWarning } = process;
    process.emitWarning = common.mustCall((...args) => {
      if (timer) {
        clearTimeout(timer);
        timer = null;
        done();
      }
      emitWarning(...args);
    }, 2);

    let timer = setTimeout(common.mustNotCall(), 10000);
  },
);

// https://github.com/nodejs/node/issues/30953
asyncTest(
  'Catching a promise should not take effect on previous promises',
  function(done) {
    onUnhandledSucceed(done, function(reason, promise) {
      assert.strictEqual(reason, '1');
    });
    Promise.reject('1');
    Promise.reject('2').catch(function() {});
  }
);
