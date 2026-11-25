'use strict';

const common = require('../common');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');
const assert = require('assert');
const { AsyncLocalStorage, createHook } = require('async_hooks');
const { Writable } = require('stream');

const mode = process.argv[2];
const hook = mode === 'hooks' ? createHook({ init() {} }).enable() : undefined;

async function checkContext(origin) {
  const storage = new AsyncLocalStorage();
  const callbacks = [];
  const consumed = [];
  let started = Promise.withResolvers();

  function hold(chunks, callback) {
    callbacks.push(() => {
      for (const chunk of chunks) consumed.push(chunk[0]);
      callback();
    });
    started.resolve();
  }

  const writable = new Writable({
    highWaterMark: 8,
    write: common.mustCallAtLeast((chunk, encoding, callback) => {
      hold([chunk], callback);
    }),
    writev: common.mustCallAtLeast((chunks, callback) => {
      hold(chunks.map(({ chunk }) => chunk), callback);
    }),
  });
  const writer = storage.run(origin, () => Writable.toWeb(writable).getWriter());
  const writes = [];
  for (let value = 1; value <= 16; value++) {
    const chunk = Buffer.from([value]);
    writes.push(storage.run(`writer-${value}`, common.mustCall(() =>
      writer.write(chunk).then(common.mustCall(() => {
        assert.strictEqual(storage.getStore(), `writer-${value}`);
        chunk.fill(255);
      })))));
  }

  let index = 0;
  while (consumed.length < 16) {
    if (callbacks.length === 0) await started.promise;
    started = Promise.withResolvers();
    const callback = callbacks.shift();
    storage.run(`callback-${index++}`, common.mustCall(() => {
      callback();
      assert.strictEqual(storage.getStore(), `callback-${index - 1}`);
    }));
    await new Promise(setImmediate);
  }
  await Promise.all(writes);
  await writer.close();
  assert.deepStrictEqual(consumed, [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]);
  storage.disable();
}

async function checkInterleavedWrites(stringWriteMode) {
  const writesString = stringWriteMode !== 'none';
  const storage = new AsyncLocalStorage();
  const callbacks = [];
  const consumed = [];
  const writable = new Writable({
    write: common.mustCall((chunk, encoding, callback) => {
      const complete = () => {
        consumed.push(chunk[0]);
        callback();
      };
      if (chunk[0] === 1) setImmediate(complete);
      else callbacks.push(complete);
    }, writesString ? 5 : 4),
  });
  const writer = Writable.toWeb(writable).getWriter();
  await writer.write(Buffer.from([1]));
  await writer.write(Buffer.from([2]));
  callbacks.shift()();

  if (stringWriteMode === 'native') {
    writable.write('3');
  } else if (stringWriteMode === 'web') {
    await writer.write('3');
  }
  await writer.write(Buffer.from([3]));
  await writer.write(Buffer.from([4]));
  if (writesString) callbacks.shift()();
  storage.run('completion-B', common.mustCall(() => {
    callbacks.shift()();
    assert.strictEqual(storage.getStore(), 'completion-B');
  }));
  callbacks.shift()();
  await writer.close();
  assert.deepStrictEqual(consumed, writesString ?
    [1, 2, '3'.charCodeAt(0), 3, 4] : [1, 2, 3, 4]);
  storage.disable();
}

async function main() {
  // node:test enables async hooks and would hide the default frame path.
  for (const origin of [undefined, 'stream-origin']) {
    await checkContext(origin);
  }
  for (const stringWriteMode of ['none', 'web', 'native']) {
    await checkInterleavedWrites(stringWriteMode);
  }
  hook?.disable();

  if (mode === undefined) {
    spawnSyncAndExitWithoutError(process.execPath, [__filename, 'hooks']);
  }
}

main().then(common.mustCall());
