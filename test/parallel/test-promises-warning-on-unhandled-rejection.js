// Flags: --no-warnings
'use strict';

// Test that warnings are emitted when a Promise experiences an uncaught
// rejection, and then again if the rejection is handled later on.

const common = require('../common');
const assert = require('assert');

let handled = false;

process.on('warning', common.mustCall((warning) => {
  if (handled === false) {
    assert.strictEqual(warning.name, 'UnhandledPromiseRejectionWarning');
    assert(/^Unhandled promise rejection/.test(warning.message));
    handled = true;
  } else {
    assert.strictEqual(warning.name, 'PromiseRejectionHandledWarning');
    assert(/^Promise rejection was handled asynchronous/.test(warning.message));
  }
}, 2));

const p = Promise.reject('This was rejected');
setImmediate(common.mustCall(() => p.catch(() => {})));
