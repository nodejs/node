'use strict';

// node::NewContext calls this script

(function(global) {
  // https://github.com/nodejs/node/issues/14909
  delete global.Intl.v8BreakIterator;

  // https://github.com/nodejs/node/issues/21219
  Object.defineProperty(global.Atomics, 'notify', {
    value: global.Atomics.wake,
    writable: true,
    enumerable: false,
    configurable: true,
  });
}(this));
