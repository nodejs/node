'use strict';

// see also test/parallel/test-handle-wrap-isrefed.js

const common = require('../common');
const strictEqual = require('assert').strictEqual;
const ReadStream = require('tty').ReadStream;
const tty = new ReadStream(0);
const isTTY = process.binding('tty_wrap').isTTY;
strictEqual(isTTY(0), true, 'tty_wrap: stdin is not a TTY');
strictEqual(Object.getPrototypeOf(tty._handle).hasOwnProperty('hasRef'),
            true, 'tty_wrap: hasRef() missing');
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
