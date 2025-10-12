// Flags: --expose-gc --no-warnings --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');
const { internalBinding } = require('internal/test/binding');
const fs = internalBinding('fs');
const { stringToFlags } = require('internal/fs/utils');

// Verifies that the FileHandle object is garbage collected and that an
// error is thrown if it is not closed.
process.on('uncaughtException', common.mustCall((err) => {
  assert.strictEqual(err.code, 'ERR_INVALID_STATE');
  assert.match(err.message, /^A FileHandle object was closed during/);
}));


{
  const ctx = {};
  fs.openFileHandle(path.toNamespacedPath(__filename),
                    stringToFlags('r'), 0o666, undefined, ctx);
  assert.strictEqual(ctx.errno, undefined);
}

globalThis.gc();

setTimeout(() => {}, 10);
