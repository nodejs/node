// Flags: --expose-internals --no-warnings
'use strict';

require('../common');

const { internalBinding } = require('internal/test/binding');
const { TTY, isTTY } = internalBinding('tty_wrap');
const strictEqual = require('assert').strictEqual;

strictEqual(isTTY(0), true, 'fd 0 is not a TTY');

const handle = new TTY(0);
handle.readStart();
handle.onread = () => {};

strictEqual(handle.hasRef(), true, 'TTY handle not initially active');

handle.unref();

strictEqual(handle.hasRef(), false, 'TTY handle active after unref()');

handle.ref();

strictEqual(handle.hasRef(), true, 'TTY handle inactive after ref()');

handle.unref();
