// This file is compiled as if it's wrapped in a function with arguments
// passed by node::NewContext()
/* global global */

'use strict';

// https://github.com/nodejs/node/issues/14909
if (global.Intl) {
  delete global.Intl.v8BreakIterator;
}

// https://github.com/nodejs/node/issues/21219
if (global.Atomics) {
  delete global.Atomics.wake;
}
