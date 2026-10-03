// Flags: --experimental-stream-iter
'use strict';

// Edge case tests for pipeToSync close and failure behavior.

const common = require('../common');
const assert = require('assert');
const {
  pipeTo, pipeToSync, fromSync, push, text,
} = require('stream/iter');

// When endSync() cannot close synchronously (-1), pipeToSync() falls back to
// end() without failing the writer: all of the data was accepted.
async function testPipeToSyncEndSyncFallsBackToEnd() {
  const writer = {
    writeSync() { return true; },
    endSync: common.mustCall(() => -1),
    end: common.mustCall(() => Promise.reject(new Error('ignored'))),
    fail: common.mustNotCall(),
  };
  assert.strictEqual(pipeToSync(fromSync('data'), writer), 4);

  // Without end(), the writer is left as endSync() left it.
  assert.strictEqual(pipeToSync(fromSync('data'), {
    writeSync() { return true; },
    endSync: common.mustCall(() => -1),
    fail: common.mustNotCall(),
  }), 4);
}

// pipeToSync() into a push() writer whose consumer has not read yet: the
// writer cannot close synchronously, but the consumer must still see all of
// the data followed by a clean end of the stream.
async function testPipeToSyncIntoPushWriter() {
  const { writer, readable } = push();
  assert.strictEqual(pipeToSync(fromSync(['abc', 'def']), writer), 6);
  assert.strictEqual(await text(readable), 'abcdef');
  assert.strictEqual(writer.endSync(), 6);
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
  testPipeToSyncEndSyncFallsBackToEnd(),
  testPipeToSyncIntoPushWriter(),
  testPipeToSyncNoEndSync(),
  testPipeToSyncPreventFail(),
  testPipeToSyncPreventClose(),
  testFailThrowingDoesNotMaskError(),
]).then(common.mustCall());
