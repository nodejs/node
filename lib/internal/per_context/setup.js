// This file is compiled as if it's wrapped in a function with arguments
// passed by node::NewContext()

'use strict';

// https://github.com/nodejs/node/issues/14909
if (typeof Intl !== 'undefined') {
  delete Intl.v8BreakIterator;
}

// https://github.com/nodejs/node/issues/21219
if (typeof Atomics !== 'undefined') {
  delete Atomics.wake;
}
