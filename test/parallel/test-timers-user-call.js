// Make sure `setTimeout()` and friends don't throw if the user-supplied
// function has .call() and .apply() monkey-patched to undesirable values.

// Refs: https://github.com/nodejs/node/issues/12956

'use strict';

const common = require('../common');

{
  const fn = common.mustCall(10);
  fn.call = 'not a function';
  fn.apply = 'also not a function';
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
    const fn = common.mustCall(() => { clearInterval(interval); });
    fn.call = 'not a function';
    fn.apply = 'also not a function';
    const interval = setInterval(fn, 1, ...args);
  };

  testInterval();
  testInterval('oneArg');
  testInterval('two', 'args');
  testInterval('three', '(3)', 'args');
  testInterval('more', 'than', 'three', 'args');
}
