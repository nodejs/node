// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('node:assert');
const { internalBinding } = require('internal/test/binding');
const { TCP, constants } = internalBinding('tcp_wrap');

// Context updates preserve the handle's identity and tolerate native destruction.
const handle = new TCP(constants.SOCKET);
const asyncId = handle.getAsyncId();
const frame = new Map();

handle.setAsyncContextFrame(frame);
assert.strictEqual(handle.getAsyncContextFrameForDebuggingOnly(), frame);
handle.setAsyncContextFrame(undefined);
assert.strictEqual(handle.getAsyncContextFrameForDebuggingOnly(), undefined);
assert.strictEqual(handle.getAsyncId(), asyncId);

handle.close(common.mustCall(() => {
  // The native wrapper is deleted after the close callback returns.
  setImmediate(common.mustCall(() => {
    handle.setAsyncContextFrame(frame);
    handle.setAsyncContextFrame(undefined);
  }));
}));
