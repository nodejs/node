// Make sure `setTimeout()` and friends don't throw if the user-supplied
// function doesn't provide valid .call() or .apply().

// Refs: https://github.com/nodejs/node/issues/12956

'use strict';

const common = require('../common');
const assert = require('node:assert');

{
  const fn = new Proxy(common.mustCall(10), {
    get(target, property, receiver) {
      // We don't want `call` and `apply` to be accessed in particular
      assert.fail(`tried to access .${property} property on fn`);
    }
  });
  setTimeout(fn, 1);
  setTimeout(fn, 1, 'oneArg');
  setTimeout(fn, 1, 'two', 'args');
  setTimeout(fn, 1, 'three', '(3)', 'args');
  setTimeout(fn, 1, 'more', 'than', 'three', 'args');

  setImmediate(fn, 1);
  setImmediate(fn, 1, 'oneArg');
  setImmediate(fn, 1, 'two', 'args');
  setImmediate(fn, 1, 'three', '(3)', 'args');
  setImmediate(fn, 1, 'more', 'than', 'three', 'args');
}

{
  const testInterval = (...args) => {
    const fn = new Proxy(common.mustCall(() => { clearInterval(interval); }), {
      get(target, property, receiver) {
        // We don't want `call` and `apply` to be accessed in particular
        assert.fail(`tried to access .${property} property on fn`);
      }
    });
    const interval = setInterval(fn, 1, ...args);
  };

  testInterval();
  testInterval('oneArg');
  testInterval('two', 'args');
  testInterval('three', '(3)', 'args');
  testInterval('more', 'than', 'three', 'args');
}
