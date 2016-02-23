'use strict';

require('../common');
const assert = require('assert');
const async_wrap = process.binding('async_wrap');
var asyncThrows = 0;
var uncaughtExceptionCount = 0;

process.on('uncaughtException', (e) => {
  assert.equal(e.message, 'oh noes!', 'error messages do not match');
});

process.on('exit', () => {
  process.removeAllListeners('uncaughtException');
  assert.equal(uncaughtExceptionCount, 1);
  assert.equal(uncaughtExceptionCount, asyncThrows);
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
