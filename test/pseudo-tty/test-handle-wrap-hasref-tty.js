// Flags: --expose-internals --no-warnings
'use strict';

// See also test/parallel/test-handle-wrap-hasref.js

const common = require('../common');
const assert = require('assert');
const ReadStream = require('tty').ReadStream;
const tty = new ReadStream(0);
const { internalBinding } = require('internal/test/binding');
const isTTY = internalBinding('tty_wrap').isTTY;
assert.ok(isTTY(0), 'tty_wrap: stdin is not a TTY');
assert.ok(tty._handle.hasRef(), 'tty_wrap: not initially refed');
tty.unref();
assert.ok(!tty._handle.hasRef(), 'tty_wrap: unref() ineffective');
tty.ref();
assert.ok(tty._handle.hasRef(), 'tty_wrap: ref() ineffective');
tty._handle.close(common.mustCall(() =>
  assert.ok(!tty._handle.hasRef(), 'tty_wrap: not unrefed on close')));
