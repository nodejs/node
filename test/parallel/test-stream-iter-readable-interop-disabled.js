'use strict';

// Tests that toAsyncStreamable throws ERR_STREAM_ITER_MISSING_FLAG
// when --experimental-stream-iter is not enabled.

const common = require('../common');
const assert = require('assert');
const { spawnPromisified } = common;

async function testToAsyncStreamableWithoutFlag() {
  const { stderr, code } = await spawnPromisified(process.execPath, [
    '-e',
    `
      const { Readable } = require('stream');
      const r = new Readable({ read() {} });
      r[Symbol.for('Stream.toAsyncStreamable')]();
    `,
  ]);
  assert.notStrictEqual(code, 0);
  assert.match(stderr, /ERR_STREAM_ITER_MISSING_FLAG/);
}

async function testToAsyncStreamableWithFlag() {
  const { code } = await spawnPromisified(process.execPath, [
    '--experimental-stream-iter',
    '-e',
    `
      const { Readable } = require('stream');
      const r = new Readable({
        read() { this.push(Buffer.from('ok')); this.push(null); }
      });
      const sym = Symbol.for('Stream.toAsyncStreamable');
      const iter = r[sym]();
      // Should not throw, and should have stream property
      if (!iter.stream) process.exit(1);
    `,
  ]);
  assert.strictEqual(code, 0);
}

async function testFromStreamIterWithoutFlag() {
  const { stderr, code } = await spawnPromisified(process.execPath, [
    '-e',
    `
      const { Readable } = require('stream');
      async function* gen() { yield [Buffer.from('x')]; }
      Readable.fromStreamIter(gen());
    `,
  ]);
  assert.notStrictEqual(code, 0);
  assert.match(stderr, /ERR_STREAM_ITER_MISSING_FLAG/);
}

async function testFromStreamIterSyncWithoutFlag() {
  const { stderr, code } = await spawnPromisified(process.execPath, [
    '-e',
    `
      const { Readable } = require('stream');
      function* gen() { yield [Buffer.from('x')]; }
      Readable.fromStreamIterSync(gen());
    `,
  ]);
  assert.notStrictEqual(code, 0);
  assert.match(stderr, /ERR_STREAM_ITER_MISSING_FLAG/);
}

Promise.all([
  testToAsyncStreamableWithoutFlag(),
  testToAsyncStreamableWithFlag(),
  testFromStreamIterWithoutFlag(),
  testFromStreamIterSyncWithoutFlag(),
]).then(common.mustCall());
