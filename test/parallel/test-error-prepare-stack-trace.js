// Flags: --enable-source-maps
'use strict';

require('../common');
const assert = require('assert');

// Error.prepareStackTrace() can be overridden with source maps enabled.
{
  let prepareCalled = false;
  Error.prepareStackTrace = (_error, trace) => {
    prepareCalled = true;
  };
  try {
    throw new Error('foo');
  } catch (err) {
    err.stack;
  }
  assert(prepareCalled);
}
