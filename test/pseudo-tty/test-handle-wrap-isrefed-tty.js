'use strict';

// see also test/parallel/test-handle-wrap-isrefed.js

const common = require('../common');
const strictEqual = require('assert').strictEqual;

function makeAssert(message) {
  return function(actual, expected) {
    strictEqual(actual, expected, message);
  };
}

const assert = makeAssert('hasRef() not working on tty_wrap');

const ReadStream = require('tty').ReadStream;
const tty = new ReadStream(0);
const isTTY = process.binding('tty_wrap').isTTY;
assert(isTTY(0), true);
assert(Object.getPrototypeOf(tty._handle).hasOwnProperty('hasRef'), true);
assert(tty._handle.hasRef(), true);
tty.unref();
assert(tty._handle.hasRef(), false);
tty.ref();
assert(tty._handle.hasRef(), true);
tty._handle.close(
    common.mustCall(() => assert(tty._handle.hasRef(), false)));
