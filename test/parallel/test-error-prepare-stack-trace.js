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

// Error.prepareStackTrace() should expose getOriginalLineNumber(),
// getOriginalColumnNumber(), getOriginalFileName().
{
  let callSite;
  Error.prepareStackTrace = (_error, trace) => {
    const throwingRequireCallSite = trace[0];
    if (throwingRequireCallSite.getFileName().endsWith('typescript-throw.js')) {
      callSite = throwingRequireCallSite;
    }
  };
  try {
    // Require a file that throws an exception, and has a source map.
    require('../fixtures/source-map/typescript-throw.js');
  } catch (err) {
    err.stack; // Force prepareStackTrace() to be called.
  }
  assert(callSite);

  assert(callSite.getFileName().endsWith('typescript-throw.js'));
  assert(callSite.getOriginalFileName().endsWith('typescript-throw.ts'));

  assert.strictEqual(callSite.getLineNumber(), 20);
  assert.strictEqual(callSite.getColumnNumber(), 15);

  assert.strictEqual(callSite.getOriginalLineNumber(), 18);
  assert.strictEqual(callSite.getOriginalColumnNumber(), 11);
}
