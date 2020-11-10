// Flags: --no-warnings
'use strict';

// Test that warnings are emitted when a Promise experiences an uncaught
// rejection, and then again if the rejection is handled later on.

const common = require('../common');
const assert = require('assert');

common.disableCrashOnUnhandledRejection();

let b = 0;

process.on('warning', common.mustCall((warning) => {
  switch (b++) {
    case 0:
      // String rejection error displayed
      assert.strictEqual(warning.message, 'This was rejected');
      break;
    case 1:
      // Warning about rejection not being handled (will be next tick)
      assert.strictEqual(warning.name, 'UnhandledPromiseRejectionWarning');
      assert(
        /Unhandled promise rejection/.test(warning.message),
        'Expected warning message to contain "Unhandled promise rejection" ' +
        `but did not. Had "${warning.message}" instead.`
      );
      break;
    case 2:
      // One time deprecation warning, first unhandled rejection
      assert.strictEqual(warning.name, 'DeprecationWarning');
      break;
    case 3:
      // Number rejection error displayed. Note it's been stringified
      assert.strictEqual(warning.message, '42');
      break;
    case 4:
      // Unhandled rejection warning (won't be handled next tick)
      assert.strictEqual(warning.name, 'UnhandledPromiseRejectionWarning');
      assert(
        /Unhandled promise rejection/.test(warning.message),
        'Expected warning message to contain "Unhandled promise rejection" ' +
        `but did not. Had "${warning.message}" instead.`
      );
      break;
    case 5:
      // Rejection handled asynchronously.
      assert.strictEqual(warning.name, 'PromiseRejectionHandledWarning');
      assert(/Promise rejection was handled asynchronously/
        .test(warning.message));
  }
}, 6));

const p = Promise.reject('This was rejected'); // Reject with a string
setImmediate(common.mustCall(() => p.catch(() => { })));
Promise.reject(42); // Reject with a number
