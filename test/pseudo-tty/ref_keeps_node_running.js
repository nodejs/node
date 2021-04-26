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

strictEqual(
  Object.values(getActiveResources('handles')).length,
  1,
  'TTY handle not initially active'
);

handle.unref();

strictEqual(
  Object.values(getActiveResources('handles')).length,
  0,
  'TTY handle not initially active'
);

handle.ref();

strictEqual(
  Object.values(getActiveResources('handles')).length,
  1,
  'TTY handle not initially active'
);

handle.unref();

strictEqual(
  Object.values(getActiveResources('handles')).length,
  0,
  'TTY handle not initially active'
);
