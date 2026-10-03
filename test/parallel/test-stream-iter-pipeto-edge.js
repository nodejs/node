// Flags: --experimental-stream-iter
'use strict';

// Edge case tests for pipeToSync close and failure behavior.

const common = require('../common');
const assert = require('assert');
const { pipeTo, pipeToSync, fromSync } = require('stream/iter');

// pipeToSync cannot complete when endSync() requires async fallback.
async function testPipeToSyncEndSyncFailure() {
  let endCalled = false;
  const writer = {
    writeSync() { return true; },
    endSync() { return -1; },
    end() { endCalled = true; },
  };
  assert.throws(
    () => pipeToSync(fromSync('data'), writer, { preventFail: true }),
    { code: 'ERR_INVALID_STATE' },
  );
  assert.strictEqual(endCalled, false);
}

// pipeToSync requires endSync() when closing is enabled.
async function testPipeToSyncNoEndSync() {
  let writeCalled = false;
  let endCalled = false;
  const writer = {
    writeSync() { writeCalled = true; return true; },
    end() { endCalled = true; },
  };
  assert.throws(
    () => pipeToSync(fromSync('data'), writer),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  assert.strictEqual(writeCalled, false);
  assert.strictEqual(endCalled, false);
}

// pipeToSync with preventFail: true — source error does NOT call fail()
async function testPipeToSyncPreventFail() {
  let failCalled = false;
  const writer = {
    writeSync() { return true; },
    endSync() { return 0; },
    fail() { failCalled = true; },
  };
  function* badSource() {
    yield [new Uint8Array([1])];
    throw new Error('source error');
  }
  assert.throws(
    () => pipeToSync(badSource(), writer, { preventFail: true }),
    { message: 'source error' },
  );
  assert.strictEqual(failCalled, false);
}

// pipeToSync with preventClose: true — end/endSync not called
async function testPipeToSyncPreventClose() {
  let endCalled = false;
  const writer = {
    writeSync() { return true; },
    endSync() { endCalled = true; return 0; },
  };
  pipeToSync(fromSync('data'), writer, { preventClose: true });
  assert.strictEqual(endCalled, false);
}

// An exception thrown by writer.fail() must not replace the error that made
// the pipe fail.
async function testFailThrowingDoesNotMaskError() {
  const cause = new Error('write failed');
  const syncWriter = {
    writeSync() { throw cause; },
    endSync: common.mustNotCall(),
    fail: common.mustCall((error) => {
      assert.strictEqual(error, cause);
      throw new Error('fail() threw');
    }),
  };
  assert.throws(() => pipeToSync(fromSync('data'), syncWriter),
                (error) => error === cause);

  const asyncWriter = {
    async write() { throw cause; },
    end: common.mustNotCall(),
    fail: common.mustCall((error) => {
      assert.strictEqual(error, cause);
      throw new Error('fail() threw');
    }),
  };
  await assert.rejects(pipeTo(fromSync('data'), asyncWriter),
                       (error) => error === cause);

  // Same for the already-aborted signal path of pipeTo().
  const signal = AbortSignal.abort();
  await assert.rejects(
    pipeTo(fromSync('data'), {
      write: common.mustNotCall(),
      fail() { throw new Error('fail() threw'); },
    }, { signal }),
    (error) => error === signal.reason);
}

Promise.all([
  testPipeToSyncEndSyncFailure(),
  testPipeToSyncNoEndSync(),
  testPipeToSyncPreventFail(),
  testPipeToSyncPreventClose(),
  testFailThrowingDoesNotMaskError(),
]).then(common.mustCall());
