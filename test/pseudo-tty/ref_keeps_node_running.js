// Flags: --expose-internals --no-warnings
'use strict';

require('../common');

const { internalBinding } = require('internal/test/binding');
const { TTY, isTTY } = internalBinding('tty_wrap');
const assert = require('assert');

assert.ok(isTTY(0), 'fd 0 is not a TTY');

const handle = new TTY(0);
handle.readStart();
handle.onread = () => {};

function isHandleActive(handle) {
  return process._getActiveHandles().some((active) => active === handle);
}

assert.ok(isHandleActive(handle), 'TTY handle not initially active');

handle.unref();

assert.ok(!isHandleActive(handle), 'TTY handle active after unref()');

handle.ref();

assert.ok(isHandleActive(handle), 'TTY handle inactive after ref()');

handle.unref();
