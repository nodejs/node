'use strict';

require('../common');

const { TTY, isTTY } = process.binding('tty_wrap');
const strictEqual = require('assert').strictEqual;

strictEqual(isTTY(0), true, 'fd 0 is not a TTY');

const handle = new TTY(0);
handle.readStart();
handle.onread = () => {};

function isHandleActive(handle) {
  return process._getActiveHandles().some((active) => active === handle);
}

strictEqual(isHandleActive(handle), true, 'TTY handle not initially active');

handle.unref();

strictEqual(isHandleActive(handle), false, 'TTY handle active after unref()');

handle.ref();

strictEqual(isHandleActive(handle), true, 'TTY handle inactive after ref()');

handle.unref();
