// Flags: --expose-internals --no-js-immutable-arraybuffer
'use strict';

const common = require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');
const assert = require('node:assert/strict');
const { PassThrough } = require('node:stream');
const { deserialize } = require('node:v8');
const { setUserBuffer } = require('internal/stream_base_commons');

assert.strictEqual(
  Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'immutable'),
  undefined,
);

const { child } = spawnSyncAndAssert(process.execPath, [
  '--js-immutable-arraybuffer',
  '-e',
  `const { serialize } = require('node:v8');
   process.stdout.write(serialize(new ArrayBuffer(8).transferToImmutable()));`,
], {});

const stream = new PassThrough();
assert.strictEqual(setUserBuffer(stream, new Uint8Array(8)), true);
stream.on('error', common.expectsError({
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
}));
const buffer = new Uint8Array(deserialize(child.stdout));
assert.strictEqual(setUserBuffer(stream, buffer, true /* deferError */), false);
