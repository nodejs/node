// Flags: --experimental-stream-iter
'use strict';

// Tests for toWritable() - creating a classic stream.Writable
// backed by a stream/iter Writer.

const common = require('../common');
const assert = require('assert');
const {
  push,
  text,
  toWritable,
} = require('stream/iter');

// =============================================================================
// Basic: write through fromStreamIter writable, read from readable
// =============================================================================

async function testBasicWrite() {
  const { writer, readable } = push({ backpressure: 'block' });
  const writable = toWritable(writer);

  writable.write('hello');
  writable.write(' world');
  writable.end();

  const result = await text(readable);
  assert.strictEqual(result, 'hello world');
}

// =============================================================================
// _write delegates to writer.write()
// =============================================================================

async function testWriteDelegatesToWriter() {
  const chunks = [];
  // Create a minimal Writer that records writes.
  const writer = {
    write(chunk) {
      chunks.push(Buffer.from(chunk));
      return Promise.resolve();
    },
    end() { return Promise.resolve(0); },
    fail() {},
  };

  const writable = toWritable(writer);

  await new Promise((resolve, reject) => {
    writable.write('hello', (err) => {
      if (err) reject(err);
      else resolve();
    });
  });

  assert.strictEqual(Buffer.concat(chunks).toString(), 'hello');
}

// =============================================================================
// _writev delegates to writer.writev() when available
// =============================================================================

async function testWritevDelegation() {
  const batches = [];
  const writer = {
    write(chunk) {
      return Promise.resolve();
    },
    writev(chunks) {
      batches.push(chunks.map((c) => Buffer.from(c)));
      return Promise.resolve();
    },
    writevSync(chunks) {
      return false;
    },
    end() { return Promise.resolve(0); },
    fail() {},
  };

  const writable = toWritable(writer);

  // Cork to batch writes, then uncork to trigger _writev
  writable.cork();
  writable.write('a');
  writable.write('b');
  writable.write('c');
  writable.uncork();

  await new Promise((resolve) => writable.end(resolve));

  // Writev should have been called with the batched chunks
  assert.ok(batches.length > 0, 'writev should have been called');
}

// =============================================================================
// _writev not defined when writer lacks writev
// =============================================================================

function testNoWritevWithoutWriterWritev() {
  const writer = {
    write(chunk) { return Promise.resolve(); },
  };

  const writable = toWritable(writer);
  // The _writev should be null (Writable default) when writer lacks writev
  assert.strictEqual(writable._writev, null);
}

// =============================================================================
// Try-sync-first: writeSync is attempted before write
// =============================================================================

async function testWriteSyncFirst() {
  let syncCalled = false;
  let asyncCalled = false;

  const writer = {
    writeSync(chunk) {
      syncCalled = true;
      return true;  // Sync path accepted
    },
    write(chunk) {
      asyncCalled = true;
      return Promise.resolve();
    },
    end() { return Promise.resolve(0); },
    fail() {},
  };

  const writable = toWritable(writer);

  await new Promise((resolve) => {
    writable.write('test', resolve);
  });

  assert.ok(syncCalled, 'writeSync should have been called');
  assert.ok(!asyncCalled, 'write should not have been called');
}

// =============================================================================
// Try-sync-first: falls back to async when writeSync returns false
// =============================================================================

async function testWriteSyncFallback() {
  let syncCalled = false;
  let asyncCalled = false;

  const writer = {
    writeSync(chunk) {
      syncCalled = true;
      return false;  // Sync path rejected
    },
    write(chunk) {
      asyncCalled = true;
      return Promise.resolve();
    },
    end() { return Promise.resolve(0); },
    fail() {},
  };

  const writable = toWritable(writer);

  await new Promise((resolve) => {
    writable.write('test', resolve);
  });

  assert.ok(syncCalled, 'writeSync should have been called');
  assert.ok(asyncCalled, 'write should have been called as fallback');
}

// =============================================================================
// Try-sync-first: endSync attempted before end
// =============================================================================

async function testEndSyncFirst() {
  let endSyncCalled = false;
  let endAsyncCalled = false;

  const writer = {
    write(chunk) { return Promise.resolve(); },
    endSync() {
      endSyncCalled = true;
      return 5;  // Success, returns byte count
    },
    end() {
      endAsyncCalled = true;
      return Promise.resolve(5);
    },
    fail() {},
  };

  const writable = toWritable(writer);

  await new Promise((resolve) => writable.end(resolve));

  assert.ok(endSyncCalled, 'endSync should have been called');
  assert.ok(!endAsyncCalled, 'end should not have been called');
}

// =============================================================================
// Try-sync-first: endSync returns -1, falls back to async end
// =============================================================================

async function testEndSyncFallback() {
  let endSyncCalled = false;
  let endAsyncCalled = false;

  const writer = {
    write(chunk) { return Promise.resolve(); },
    endSync() {
      endSyncCalled = true;
      return -1;  // Can't complete synchronously
    },
    end() {
      endAsyncCalled = true;
      return Promise.resolve(0);
    },
    fail() {},
  };

  const writable = toWritable(writer);

  await new Promise((resolve) => writable.end(resolve));

  assert.ok(endSyncCalled, 'endSync should have been called');
  assert.ok(endAsyncCalled, 'end should have been called as fallback');
}

// =============================================================================
// _final delegates to writer.end()
// =============================================================================

async function testFinalDelegatesToEnd() {
  let endCalled = false;
  const writer = {
    write(chunk) { return Promise.resolve(); },
    end() {
      endCalled = true;
      return Promise.resolve(0);
    },
    fail() {},
  };

  const writable = toWritable(writer);

  await new Promise((resolve) => writable.end(resolve));

  assert.ok(endCalled, 'writer.end() should have been called');
}

// =============================================================================
// _destroy delegates to writer.fail()
// =============================================================================

async function testDestroyDelegatesToFail() {
  let failReason = null;
  const writer = {
    write(chunk) { return Promise.resolve(); },
    end() { return Promise.resolve(0); },
    fail(reason) { failReason = reason; },
  };

  const writable = toWritable(writer);
  writable.on('error', () => {});  // Prevent unhandled

  const testErr = new Error('destroy test');
  writable.destroy(testErr);

  // Give a tick for destroy to propagate
  await new Promise((resolve) => setTimeout(resolve, 10));

  assert.strictEqual(failReason, testErr);
}

// =============================================================================
// Error from writer.write() propagates to writable
// =============================================================================

async function testWriteErrorPropagation() {
  const writer = {
    write(chunk) {
      return Promise.reject(new Error('write failed'));
    },
    end() { return Promise.resolve(0); },
    fail() {},
  };

  const writable = toWritable(writer);

  await assert.rejects(new Promise((resolve, reject) => {
    writable.write('data', (err) => {
      if (err) reject(err);
      else resolve();
    });
  }), { message: 'write failed' });
}

// =============================================================================
// Invalid writer argument throws
// =============================================================================

function testInvalidWriterThrows() {
  assert.throws(
    () => toWritable(null),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  assert.throws(
    () => toWritable({}),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  assert.throws(
    () => toWritable('not a writer'),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  // Object with write is valid (only write is required).
  // This should not throw.
  toWritable({
    write() { return Promise.resolve(); },
  });
}

// =============================================================================
// Round-trip: push writer -> fromStreamIter -> write -> read from readable
// =============================================================================

async function testRoundTrip() {
  const { writer, readable } = push({ backpressure: 'block' });
  const writable = toWritable(writer);

  const data = 'round trip test data';
  writable.write(data);
  writable.end();

  const result = await text(readable);
  assert.strictEqual(result, data);
}

// =============================================================================
// Multiple sequential writes
// =============================================================================

async function testSequentialWrites() {
  const { writer, readable } = push({ backpressure: 'block' });
  const writable = toWritable(writer);

  for (let i = 0; i < 10; i++) {
    writable.write(`chunk${i}`);
  }
  writable.end();

  let expected = '';
  for (let i = 0; i < 10; i++) {
    expected += `chunk${i}`;
  }
  const result = await text(readable);
  assert.strictEqual(result, expected);
}

// =============================================================================
// Sync callback is deferred via queueMicrotask
// =============================================================================

async function testSyncCallbackDeferred() {
  let callbackTick = false;

  const writer = {
    writeSync(chunk) {
      return true;
    },
    write(chunk) {
      return Promise.resolve();
    },
    end() { return Promise.resolve(0); },
    fail() {},
  };

  const writable = toWritable(writer);

  const p = new Promise((resolve) => {
    writable.write('test', () => {
      callbackTick = true;
      resolve();
    });
    // Callback should NOT have fired synchronously
    assert.strictEqual(callbackTick, false);
  });

  await p;
  assert.strictEqual(callbackTick, true);
}

// =============================================================================
// Minimal writer: only write() is required
// =============================================================================

async function testMinimalWriter() {
  const chunks = [];
  const writer = {
    write(chunk) {
      chunks.push(Buffer.from(chunk));
      return Promise.resolve();
    },
    // No end, fail, writeSync, writev, etc.
  };

  const writable = toWritable(writer);

  await new Promise((resolve) => {
    writable.write('minimal');
    writable.end(resolve);
  });

  assert.strictEqual(Buffer.concat(chunks).toString(), 'minimal');
}

// =============================================================================
// Destroy without error does not call fail()
// =============================================================================

async function testDestroyWithoutError() {
  let failCalled = false;
  const writer = {
    write(chunk) { return Promise.resolve(); },
    fail() { failCalled = true; },
  };

  const writable = toWritable(writer);
  writable.destroy();

  await new Promise((resolve) => setTimeout(resolve, 10));

  assert.ok(!failCalled, 'fail should not be called on clean destroy');
}

// =============================================================================
// Destroy with error calls fail() when available
// =============================================================================

async function testDestroyWithError() {
  let failReason = null;
  const writer = {
    write(chunk) { return Promise.resolve(); },
    fail(reason) { failReason = reason; },
  };

  const writable = toWritable(writer);
  writable.on('error', () => {});

  const err = new Error('test');
  writable.destroy(err);

  await new Promise((resolve) => setTimeout(resolve, 10));

  assert.strictEqual(failReason, err);
}

// =============================================================================
// Destroy with error when writer lacks fail()
// =============================================================================

async function testDestroyWithoutFail() {
  const writer = {
    write(chunk) { return Promise.resolve(); },
    // No fail method
  };

  const writable = toWritable(writer);
  writable.on('error', () => {});

  // Should not throw even though writer has no fail()
  writable.destroy(new Error('test'));

  await new Promise((resolve) => setTimeout(resolve, 10));
  assert.ok(writable.destroyed);
}

// =============================================================================
// Custom highWaterMark option
// =============================================================================

function testHighWaterMarkIsMaxSafeInt() {
  const writer = {
    write(chunk) { return Promise.resolve(); },
  };

  // HWM is set to MAX_SAFE_INTEGER to disable Writable's internal
  // buffering. The underlying Writer manages backpressure directly.
  const writable = toWritable(writer);
  assert.strictEqual(writable.writableHighWaterMark, Number.MAX_SAFE_INTEGER);
}

// =============================================================================
// writeSync throws -- falls back to async
// =============================================================================

async function testWriteSyncThrowsFallback() {
  let asyncCalled = false;

  const writer = {
    writeSync() {
      throw new Error('sync broken');
    },
    write(chunk) {
      asyncCalled = true;
      return Promise.resolve();
    },
    end() { return Promise.resolve(0); },
    fail() {},
  };

  const writable = toWritable(writer);

  await new Promise((resolve) => {
    writable.write('test', resolve);
  });

  assert.ok(asyncCalled, 'async write should be called as fallback');
}

// =============================================================================
// =============================================================================
// writer.write() throws synchronously -- error propagates to callback
// =============================================================================

async function testWriteThrowsSyncPropagation() {
  const writer = {
    write() {
      throw new Error('sync throw from write');
    },
  };

  const writable = toWritable(writer);

  await assert.rejects(new Promise((resolve, reject) => {
    writable.write('data', (err) => {
      if (err) reject(err);
      else resolve();
    });
  }), { message: 'sync throw from write' });
}

// =============================================================================
// writer.end() throws synchronously -- error propagates to callback
// =============================================================================

async function testEndThrowsSyncPropagation() {
  const writer = {
    write(chunk) { return Promise.resolve(); },
    endSync() { return -1; },
    end() {
      throw new Error('sync throw from end');
    },
  };

  const writable = toWritable(writer);
  writable.on('error', () => {});

  await new Promise((resolve) => {
    writable.end(common.mustCall((err) => {
      assert.ok(err);
      assert.strictEqual(err.message, 'sync throw from end');
      resolve();
    }));
  });
}

// =============================================================================
// Run all tests
// =============================================================================

testInvalidWriterThrows();
testNoWritevWithoutWriterWritev();
testHighWaterMarkIsMaxSafeInt();

Promise.all([
  testBasicWrite(),
  testWriteDelegatesToWriter(),
  testWritevDelegation(),
  testWriteSyncFirst(),
  testWriteSyncFallback(),
  testWriteSyncThrowsFallback(),
  testEndSyncFirst(),
  testEndSyncFallback(),
  testFinalDelegatesToEnd(),
  testDestroyDelegatesToFail(),
  testDestroyWithoutError(),
  testDestroyWithError(),
  testDestroyWithoutFail(),
  testWriteErrorPropagation(),
  testWriteThrowsSyncPropagation(),
  testEndThrowsSyncPropagation(),
  testRoundTrip(),
  testSequentialWrites(),
  testSyncCallbackDeferred(),
  testMinimalWriter(),
]).then(common.mustCall());
