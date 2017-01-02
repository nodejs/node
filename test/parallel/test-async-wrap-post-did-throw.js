'use strict';

const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const assert = require('assert');
const async_wrap = process.binding('async_wrap');
let asyncThrows = 0;
let uncaughtExceptionCount = 0;

process.on('uncaughtException', (e) => {
  assert.strictEqual(e.message, 'oh noes!', 'error messages do not match');
});

process.on('exit', () => {
  process.removeAllListeners('uncaughtException');
  assert.strictEqual(uncaughtExceptionCount, 1);
  assert.strictEqual(uncaughtExceptionCount, asyncThrows);
});

function init() { }
function post(id, threw) {
  if (threw)
    uncaughtExceptionCount++;
}

async_wrap.setupHooks({ init, post });
async_wrap.enable();

// Timers still aren't supported, so use crypto API.
// It's also important that the callback not happen in a nextTick, like many
// error events in core.
require('crypto').randomBytes(0, () => {
  asyncThrows++;
  throw new Error('oh noes!');
});
