// Flags: --expose-internals --no-warnings
'use strict';

require('../common');

const { internalBinding } = require('internal/test/binding');
const { TTY, isTTY } = internalBinding('tty_wrap');
const strictEqual = require('assert').strictEqual;
const { getActiveResources } = require('async_hooks');

strictEqual(isTTY(0), true, 'fd 0 is not a TTY');

const handle = new TTY(0);
handle.readStart();
handle.onread = () => {};

function isHandleActive(handle) {
  return Object.values(getActiveResources())
    .some((active) => active === handle);
}

strictEqual(isHandleActive(handle), true, 'TTY handle not initially active');

handle.unref();

strictEqual(isHandleActive(handle), false, 'TTY handle active after unref()');

handle.ref();

strictEqual(isHandleActive(handle), true, 'TTY handle inactive after ref()');

handle.unref();
