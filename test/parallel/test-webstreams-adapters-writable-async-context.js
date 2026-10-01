'use strict';

const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage, AsyncResource, createHook } = require('async_hooks');
const { Writable } = require('stream');

const mode = process.argv[2];
const hook = mode === 'hooks' ? createHook({ init() {} }).enable() : undefined;

async function checkContext(origin, highWaterMark) {
  const storage = new AsyncLocalStorage();
  const started = [Promise.withResolvers(), Promise.withResolvers()];
  const callbacks = [];
  const writable = new Writable({
    highWaterMark,
    write: common.mustCall((chunk, encoding, callback) => {
      // A completion callback from another request must not change the
      // context in which the adapter invokes the next queued native write.
      assert.strictEqual(storage.getStore(), origin);
      const index = chunk[0] - 1;
      callbacks[index] = callback;
      started[index].resolve();
    }, 2),
  });
  const writer = storage.run(origin, () => Writable.toWeb(writable).getWriter());
  const writes = [];
  for (const id of [1, 2]) {
    writes.push(storage.run(`writer-${id}`, common.mustCall(() =>
      writer.write(Buffer.from([id])).then(common.mustCall(() => {
        assert.strictEqual(storage.getStore(), `writer-${id}`);
      })))));
  }

  for (let index = 0; index < writes.length; index++) {
    await started[index].promise;
    storage.run(`callback-${index}`, common.mustCall(() => {
      callbacks[index]();
      assert.strictEqual(storage.getStore(), `callback-${index}`);
    }));
    await writes[index];
  }
  await writer.close();
  storage.disable();
}

async function checkMicrotaskOrder(highWaterMark) {
  const events = [];
  const started = [Promise.withResolvers(), Promise.withResolvers()];
  const callbacks = [];
  const writable = new Writable({
    highWaterMark,
    write: common.mustCall((chunk, encoding, callback) => {
      const index = chunk[0] - 1;
      events.push(`write-${index + 1}`);
      callbacks[index] = callback;
      started[index].resolve();
    }, 2),
  });
  const writer = Writable.toWeb(writable).getWriter();
  const first = writer.write(Buffer.from([1])).then(() => events.push('fulfilled-1'));
  const second = writer.write(Buffer.from([2]));
  await started[0].promise;
  queueMicrotask(() => events.push('before-callback'));
  callbacks[0]();
  queueMicrotask(() => events.push('after-callback'));
  await first;
  assert.deepStrictEqual(events, [
    'write-1', 'before-callback', 'write-2', 'after-callback', 'fulfilled-1',
  ]);
  await started[1].promise;
  callbacks[1]();
  await second;
  await writer.close();
}

async function checkLateHook(sameContext, fail) {
  const storage = new AsyncLocalStorage();
  const started = [Promise.withResolvers(), Promise.withResolvers()];
  const callbacks = [];
  const error = fail ? new Error('late hook write failure') : undefined;
  const writable = new Writable({
    write: common.mustCall((chunk, encoding, callback) => {
      assert.strictEqual(storage.getStore(), 'stream-origin');
      const index = chunk[0] - 1;
      callbacks[index] = callback;
      started[index].resolve();
    }, fail ? 1 : 2),
  });
  let writer;
  let resource;
  storage.run('stream-origin', () => {
    resource = new AsyncResource('WritableCompletion');
    writer = Writable.toWeb(writable).getWriter();
  });
  const first = writer.write(Buffer.from([1]));
  const completion = fail ? assert.rejects(first, (actual) => actual === error) : first;
  const second = fail ? undefined : writer.write(Buffer.from([2]));
  const closed = fail ? assert.rejects(writer.closed, (actual) => actual === error) : undefined;
  await started[0].promise;

  let armed = false;
  const lateHook = createHook({
    init(id, type) {
      if (armed && type === 'PROMISE') {
        storage.enterWith('hook-origin');
      }
    },
  }).enable();

  function finish(index) {
    const invoke = common.mustCall(() => {
      armed = true;
      try {
        callbacks[index](error);
      } finally {
        armed = false;
      }
      assert.strictEqual(storage.getStore(), sameContext ? 'stream-origin' : 'callback-origin');
    });
    if (sameContext) {
      resource.runInAsyncScope(invoke);
    } else {
      storage.run('callback-origin', invoke);
    }
  }

  try {
    finish(0);
    await completion;
    if (fail) {
      await closed;
    } else {
      await started[1].promise;
      finish(1);
      await second;
      await writer.close();
    }
  } finally {
    lateHook.disable();
    resource.emitDestroy();
    storage.disable();
  }
}

async function main() {
  // Keep this test outside node:test so its async hooks do not hide the
  // adapter's default completion path.
  for (const highWaterMark of [1, 65536]) {
    for (const origin of [undefined, 'stream-origin']) {
      await checkContext(origin, highWaterMark);
    }
    await checkMicrotaskOrder(highWaterMark);
  }
  for (const sameContext of [false, true]) {
    for (const fail of [false, true]) {
      await checkLateHook(sameContext, fail);
    }
  }
  hook?.disable();

  if (mode === undefined) {
    for (const args of [
      [__filename, 'hooks'],
      ['--no-async-context-frame', __filename, 'legacy'],
    ]) {
      const { code, signal, stderr } = await common.spawnPromisified(process.execPath, args);
      assert.strictEqual(code, 0, stderr);
      assert.strictEqual(signal, null);
    }
  }
}

main().then(common.mustCall());
