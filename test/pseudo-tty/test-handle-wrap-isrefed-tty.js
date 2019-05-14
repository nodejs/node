// Flags: --expose-internals --no-warnings
'use strict';

// See also test/parallel/test-handle-wrap-isrefed.js

const common = require('../common');
const strictEqual = require('assert').strictEqual;
const ReadStream = require('tty').ReadStream;
const tty = new ReadStream(0);
const { internalBinding } = require('internal/test/binding');
const isTTY = internalBinding('tty_wrap').isTTY;
strictEqual(isTTY(0), true, 'tty_wrap: stdin is not a TTY');
strictEqual(tty._handle.hasRef(),
            true, 'tty_wrap: not initially refed');
tty.unref();
strictEqual(tty._handle.hasRef(),
            false, 'tty_wrap: unref() ineffective');
tty.ref();
strictEqual(tty._handle.hasRef(),
            true, 'tty_wrap: ref() ineffective');
tty._handle.close(common.mustCall(() =>
  strictEqual(tty._handle.hasRef(),
              false, 'tty_wrap: not unrefed on close')));
