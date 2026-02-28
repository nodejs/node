// Flags: --expose-gc --no-warnings --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');
const { internalBinding } = require('internal/test/binding');
const fs = internalBinding('fs');
const { stringToFlags } = require('internal/fs/utils');

const filepath = path.toNamespacedPath(__filename);

// Verifies that the FileHandle object is garbage collected and that an
// error is thrown if it is not closed.
process.on('uncaughtException', common.mustCall((err) => {
  assert.strictEqual(err.code, 'ERR_INVALID_STATE');
  assert.match(err.message, /^A FileHandle object was closed during/);
  assert.match(err.message, new RegExp(RegExp.escape(filepath)));
}));


{
  const ctx = {};
  fs.openFileHandle(filepath,
                    stringToFlags('r'), 0o666, undefined, ctx);
  assert.strictEqual(ctx.errno, undefined);
}

globalThis.gc();

setTimeout(() => {}, 10);
