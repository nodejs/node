'use strict';

const common = require('../common');
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
    assert.strictEqual(storage.getStore(), origin);
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

async function checkIndependentBatches(withString) {
  const storage = new AsyncLocalStorage();
  const callbacks = [];
  const contexts = [];
  const writable = new Writable({
    write: common.mustCall((chunk, encoding, callback) => {
      contexts.push(storage.getStore());
      if (chunk[0] === 1) setImmediate(callback);
      else callbacks.push(callback);
    }, withString ? 5 : 4),
  });
  const writer = Writable.toWeb(writable).getWriter();
  await writer.write(Buffer.from([1]));
  await storage.run('batch-A', () => writer.write(Buffer.from([2])));
  callbacks.shift()();

  if (withString === 'native') {
    storage.run('batch-B', () => writable.write('3'));
  } else if (withString) {
    await storage.run('batch-B', () => writer.write('3'));
  }
  await storage.run('batch-B', () => writer.write(Buffer.from([3])));
  await storage.run('batch-B', () => writer.write(Buffer.from([4])));
  if (withString) storage.run('batch-B', () => callbacks.shift()());
  storage.run('completion-B', common.mustCall(() => {
    callbacks.shift()();
    assert.strictEqual(storage.getStore(), 'completion-B');
  }));
  callbacks.shift()();
  await writer.close();
  assert.deepStrictEqual(contexts, withString ?
    [undefined, 'batch-A', 'batch-B', 'batch-B', 'batch-B'] :
    [undefined, 'batch-A', 'batch-B', 'batch-B']);
  storage.disable();
}

async function checkResourceInitHook(hookFirst) {
  let storage;
  const initHook = createHook({
    init(id, type) {
      if (type === 'WEBSTREAM_WRITABLE_WRITE') storage.enterWith('hook-origin');
    },
  });
  if (hookFirst) initHook.enable();
  storage = new AsyncLocalStorage();
  const contexts = [];
  const writable = new Writable({
    write: common.mustCall((chunk, encoding, callback) => {
      contexts.push(storage.getStore());
      setImmediate(callback);
    }, 2),
  });
  const writer = Writable.toWeb(writable).getWriter();
  await writer.write(Buffer.from([1]));
  if (!hookFirst) initHook.enable();
  try {
    const write = storage.run('stream-origin', () => writer.write(Buffer.from([2])));
    assert.strictEqual(storage.getStore(), undefined);
    await write;
    await writer.close();
    assert.deepStrictEqual(contexts, [undefined, 'stream-origin']);
  } finally {
    initHook.disable();
    storage.disable();
  }
}

async function main() {
  // node:test enables async hooks and would hide the default frame path.
  for (const origin of [undefined, 'stream-origin']) {
    await checkContext(origin);
  }
  for (const withString of [false, true, 'native']) await checkIndependentBatches(withString);
  for (const hookFirst of [false, true]) await checkResourceInitHook(hookFirst);
  hook?.disable();

  if (mode === undefined) {
    const { code, signal, stderr } = await common.spawnPromisified(process.execPath, [__filename, 'hooks']);
    assert.strictEqual(code, 0, stderr);
    assert.strictEqual(signal, null);
  }
}

main().then(common.mustCall());
