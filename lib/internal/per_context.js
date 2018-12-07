// This file is compiled as if it's wrapped in a function with arguments
// passed by node::NewContext()
/* global global */

'use strict';

// https://github.com/nodejs/node/issues/14909
if (global.Intl) delete global.Intl.v8BreakIterator;

// https://github.com/nodejs/node/issues/21219

// https://github.com/v8/v8/commit/c79206b363 adds Atomics.notify so
// now we alias Atomics.wake to notify so that we can remove it
// semver major without worrying about V8.
// The warning for Atomics.wake access is added in the main bootstrap itself
function wake(typedArray, index, count) {
  return global.Reflect.apply(global.Atomics.notify, this, arguments);
}
global.Object.defineProperties(global.Atomics, {
  wake: {
    value: wake,
    writable: true,
    enumerable: false,
    configurable: true,
  },
});
