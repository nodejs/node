// Flags: --no-warnings --unhandled-rejections=warn
'use strict';

// Test that warnings are emitted when a Promise experiences an uncaught
// rejection, and then again if the rejection is handled later on.

const common = require('../common');
const assert = require('assert');

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
      assert.match(warning.message, /Unhandled promise rejection/);
      break;
    case 2:
      // Number rejection error displayed. Note it's been stringified
      assert.strictEqual(warning.message, '42');
      break;
    case 3:
      // Unhandled rejection warning (won't be handled next tick)
      assert.strictEqual(warning.name, 'UnhandledPromiseRejectionWarning');
      assert.match(warning.message, /Unhandled promise rejection/);
      break;
    case 4:
      // Rejection handled asynchronously.
      assert.strictEqual(warning.name, 'PromiseRejectionHandledWarning');
      assert.match(warning.message, /Promise rejection was handled asynchronously/);
  }
}, 5));

const p = Promise.reject('This was rejected'); // Reject with a string
setImmediate(common.mustCall(() => p.catch(() => { })));
Promise.reject(42); // Reject with a number
