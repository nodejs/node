// Flags: --experimental-stream-iter
'use strict';

// Tests for fromWritable() error, close, and end edge cases.

const common = require('../common');
const assert = require('assert');
const { EventEmitter, once } = require('events');
const { Writable } = require('stream');
const { setImmediate } = require('timers/promises');
const { fromWritable, ondrain } = require('stream/iter');

function createDuckWritable(methods) {
  const writable = new EventEmitter();
  writable.write = () => true;
  writable.end = () => {};
  writable.destroy = () => {};
  return Object.assign(writable, methods);
}

function assertNoTerminalListeners(writable) {
  assert.strictEqual(writable.listenerCount('error'), 0);
  assert.strictEqual(writable.listenerCount('finish'), 0);
  assert.strictEqual(writable.listenerCount('close'), 0);
}

// A queued write that throws while being flushed on drain rejects on its own;
// later queued writes are still committed.
async function testQueuedWriteThrowsDuringFlush() {
  const reason = new Error('queued write failed');
  const written = [];
  let calls = 0;
  const writable = createDuckWritable({
    write(chunk) {
      calls++;
      if (calls === 1) return false;
      if (calls === 2) throw reason;
      written.push(Buffer.from(chunk).toString());
      return true;
    },
  });
  const writer = fromWritable(writable, { backpressure: 'unbounded' });

  await writer.write('a');
  const second = writer.write('b');
  const third = writer.write('c');
  writable.emit('drain');

  await assert.rejects(second, (error) => error === reason);
  await third;
  assert.deepStrictEqual(written, ['c']);
}

// A queued write that errors the Writable while being flushed rejects with
// that error, as does every write still queued behind it.
async function testQueuedWriteErrorsDuringFlush() {
  const reason = new Error('queued write errored');
  let calls = 0;
  const writable = createDuckWritable({
    write() {
      calls++;
      if (calls === 1) return false;
      writable.emit('error', reason);
      return true;
    },
  });
  const writer = fromWritable(writable, { backpressure: 'unbounded' });

  await writer.write('a');
  const second = writer.write('b');
  const third = writer.write('c');
  writable.emit('drain');

  await assert.rejects(second, (error) => error === reason);
  await assert.rejects(third, (error) => error === reason);
  await assert.rejects(writer.end(), (error) => error === reason);
  assert.strictEqual(calls, 2);
  assertNoTerminalListeners(writable);
}

// A Writable that errors synchronously inside write() rejects the write that
// triggered it.
async function testWriteErrorsSynchronously() {
  for (const method of ['write', 'writev']) {
    const reason = new Error(`sync ${method} error`);
    let emitted = false;
    const writable = createDuckWritable({
      write() {
        if (!emitted) {
          emitted = true;
          writable.emit('error', reason);
        }
        return true;
      },
    });
    const writer = fromWritable(writable);
    const chunk = method === 'write' ? 'a' : ['a', 'b'];
    await assert.rejects(writer[method](chunk),
                         (error) => error === reason);
    assertNoTerminalListeners(writable);
  }
}

async function testWritevThrowRejects() {
  const reason = new Error('writev failed');
  const writable = createDuckWritable({
    write() { throw reason; },
  });
  const writer = fromWritable(writable);

  await assert.rejects(writer.writev(['a', 'b']), (error) => error === reason);
  writer.fail();
}

async function testEmptyWritev() {
  const writable = new Writable({ write: common.mustNotCall() });
  const writer = fromWritable(writable);

  await writer.writev([]);
  assert.strictEqual(await writer.end(), 0);
}

// Ending the Writable directly while writes are queued in the adapter rejects
// them, since they can no longer be committed, and settles drain waiters.
async function testExternalEndRejectsQueuedWrites() {
  const callbacks = [];
  const writable = new Writable({
    highWaterMark: 1,
    write(chunk, encoding, cb) { callbacks.push(cb); },
  });
  const writer = fromWritable(writable, { backpressure: 'unbounded' });

  await writer.write('a');
  const queued = writer.write('b');
  const drained = ondrain(writer);
  writable.end();
  callbacks.shift()();

  await assert.rejects(queued, { name: 'AbortError' });
  assert.strictEqual(await drained, false);
  assert.strictEqual(callbacks.length, 0);
}

// A Writable that errored before the adapter was created is detected
// synchronously; the adapter keeps its listeners until 'close'.
async function testErroredBeforeAdapterReleasesListenersOnClose() {
  const reason = new Error('write failed');
  const writable = new Writable({
    autoDestroy: false,
    write(chunk, encoding, cb) { cb(reason); },
  });
  writable.on('error', common.mustCall());
  writable.write('a');
  await setImmediate();

  const writer = fromWritable(writable);
  await assert.rejects(writer.write('b'), (error) => error === reason);
  assert.strictEqual(writable.listenerCount('close'), 1);

  writable.destroy();
  await setImmediate();
  assert.strictEqual(writable.listenerCount('close'), 0);
  assert.strictEqual(writable.listenerCount('finish'), 0);
}

// Duck-typed Writables may report completion only through writableFinished
// and 'close', without emitting 'finish'.
async function testCloseAfterFinishWithoutFinishEvent() {
  const writable = createDuckWritable({
    end() {
      process.nextTick(() => {
        writable.writableFinished = true;
        writable.emit('close');
      });
    },
  });
  const writer = fromWritable(writable);

  await writer.write('ab');
  assert.strictEqual(await writer.end(), 2);
  assertNoTerminalListeners(writable);
}

// A 'close' without any indication of finishing is a premature close.
async function testDuckPrematureClose() {
  const writable = createDuckWritable({
    end() {
      process.nextTick(() => writable.emit('close'));
    },
  });
  const writer = fromWritable(writable);

  await assert.rejects(writer.end(), { name: 'AbortError' });
  assertNoTerminalListeners(writable);
}

async function testAlreadyFinished() {
  const writable = new Writable({ write(chunk, encoding, cb) { cb(); } });
  writable.end();
  await once(writable, 'finish');

  const writer = fromWritable(writable);
  assert.strictEqual(writer.canWrite, null);
  await assert.rejects(writer.write('a'),
                       { code: 'ERR_STREAM_WRITE_AFTER_END' });
  assert.strictEqual(await writer.end(), 0);
  assertNoTerminalListeners(writable);
}

async function testAlreadyDestroyed() {
  const writable = new Writable({ write: common.mustNotCall() });
  writable.destroy();
  await once(writable, 'close');

  const writer = fromWritable(writable);
  assert.strictEqual(writer.canWrite, null);
  await assert.rejects(writer.write('a'),
                       { code: 'ERR_STREAM_WRITE_AFTER_END' });
  assertNoTerminalListeners(writable);
}

// When end() and the destroy() that follows both throw, the end() failure
// is reported and the adapter's listeners are released.
async function testEndAndDestroyThrow() {
  const endError = new Error('end failed');
  const writable = createDuckWritable({
    end() { throw endError; },
    destroy() { throw new Error('destroy failed'); },
  });
  const writer = fromWritable(writable);

  await assert.rejects(writer.end(), (error) => error === endError);
  assertNoTerminalListeners(writable);
}

async function testFailDestroyThrows() {
  const reason = new Error('fail reason');
  const destroyError = new Error('destroy failed');
  const writable = createDuckWritable({
    destroy() { throw destroyError; },
  });
  const writer = fromWritable(writable);

  assert.throws(() => writer.fail(reason), (error) => error === destroyError);
  assertNoTerminalListeners(writable);
  await assert.rejects(writer.write('a'), (error) => error === reason);
}

// An end() with a signal rejects with the Writable's error if ending fails.
async function testFinalErrorRejectsSignaledEnd() {
  const reason = new Error('final failed');
  const writable = new Writable({
    write(chunk, encoding, cb) { cb(); },
    final(cb) { cb(reason); },
  });
  writable.on('error', common.mustCall());
  const writer = fromWritable(writable);
  const { signal } = new AbortController();

  await writer.write('a');
  await assert.rejects(writer.end({ signal }), (error) => error === reason);
}

// The signal can be aborted synchronously by the Writable's own end(), after
// end() has checked it but before it waits on it.
async function testSignalAbortedByUnderlyingEnd() {
  const controller = new AbortController();
  const writable = createDuckWritable({
    end: common.mustCall(() => controller.abort('stop')),
  });
  const writer = fromWritable(writable);

  await assert.rejects(writer.end({ signal: controller.signal }),
                       (reason) => reason === 'stop');
  const ending = writer.end();
  writable.emit('finish');
  assert.strictEqual(await ending, 0);
}

Promise.all([
  testQueuedWriteThrowsDuringFlush(),
  testQueuedWriteErrorsDuringFlush(),
  testWriteErrorsSynchronously(),
  testWritevThrowRejects(),
  testEmptyWritev(),
  testExternalEndRejectsQueuedWrites(),
  testErroredBeforeAdapterReleasesListenersOnClose(),
  testCloseAfterFinishWithoutFinishEvent(),
  testDuckPrematureClose(),
  testAlreadyFinished(),
  testAlreadyDestroyed(),
  testEndAndDestroyThrow(),
  testFailDestroyThrows(),
  testFinalErrorRejectsSignaledEnd(),
  testSignalAbortedByUnderlyingEnd(),
]).then(common.mustCall());
