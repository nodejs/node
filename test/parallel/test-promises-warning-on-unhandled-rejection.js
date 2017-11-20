// Flags: --no-warnings
'use strict';

// Test that warnings are emitted when a Promise experiences an uncaught
// rejection, and then again if the rejection is handled later on.

const common = require('../common');
const assert = require('assert');

let b = 0;

process.on('warning', common.mustCall((warning) => {
  switch (b++) {
    case 0:
      assert.strictEqual(warning.message, 'This was rejected');
      break;
    case 1:
      assert.strictEqual(warning.name, 'UnhandledPromiseRejectionWarning');
      assert(/Unhandled promise rejection/.test(warning.message), 'Expected warning message to contain "Unhandled promise rejection" but did not. Had "' + warning.message + '" instead.');
      break;
    case 2:
      assert.strictEqual(warning.name, 'DeprecationWarning');
      break;
    case 3:
      assert.strictEqual(warning.name, 'PromiseRejectionHandledWarning');
      assert(/Promise rejection was handled asynchronously/
        .test(warning.message));
  }
}, 4));

const p = Promise.reject('This was rejected');
setImmediate(common.mustCall(() => p.catch(() => {})));
