// Flags: --experimental-stream-iter
'use strict';

// Edge case tests for pipeToSync close and failure behavior.

const common = require('../common');
const assert = require('assert');
const {
  pipeTo, pipeToSync, fromSync, push, text,
} = require('stream/iter');

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

// The data was accepted, so endSync() returning -1 does not fail the writer,
// even without preventFail, and the caller can still close it.
async function testPipeToSyncEndSyncFailureDoesNotFailWriter() {
  const writer = {
    writeSync() { return true; },
    endSync: common.mustCall(() => -1),
    end: common.mustNotCall(),
    fail: common.mustNotCall(),
  };
  assert.throws(() => pipeToSync(fromSync('data'), writer),
                { code: 'ERR_INVALID_STATE' });

  // A push() writer whose consumer has not read yet cannot close
  // synchronously. After the throw, the data is intact and the writer can be
  // ended asynchronously.
  const { writer: pushWriter, readable } = push();
  assert.throws(() => pipeToSync(fromSync(['abc', 'def']), pushWriter),
                { code: 'ERR_INVALID_STATE' });
  const result = text(readable);
  assert.strictEqual(await pushWriter.end(), 6);
  assert.strictEqual(await result, 'abcdef');
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

// failOnIncompleteClose fails a writer that cannot be closed synchronously,
// e.g. a sync-only writer that has no end().
async function testPipeToSyncFailOnIncompleteClose() {
  let failReason;
  const writer = {
    writeSync() { return true; },
    endSync: common.mustCall(() => -1),
    fail: common.mustCall((reason) => { failReason = reason; }),
  };
  assert.throws(
    () => pipeToSync(fromSync('data'), writer, { failOnIncompleteClose: true }),
    (error) => {
      assert.strictEqual(error.code, 'ERR_INVALID_STATE');
      assert.strictEqual(error, failReason);
      return true;
    });

  // preventFail takes precedence.
  assert.throws(
    () => pipeToSync(fromSync('data'), {
      writeSync() { return true; },
      endSync: common.mustCall(() => -1),
      fail: common.mustNotCall(),
    }, { failOnIncompleteClose: true, preventFail: true }),
    { code: 'ERR_INVALID_STATE' });

  // It has no effect when the writer closes synchronously.
  assert.strictEqual(pipeToSync(fromSync('data'), {
    writeSync() { return true; },
    endSync: common.mustCall(() => 4),
    fail: common.mustNotCall(),
  }, { failOnIncompleteClose: true }), 4);

  // A push() writer is failed with the error, so its consumer sees it.
  const { writer: pushWriter, readable } = push();
  let thrown;
  assert.throws(() => {
    try {
      pipeToSync(fromSync('abc'), pushWriter, { failOnIncompleteClose: true });
    } catch (error) {
      thrown = error;
      throw error;
    }
  }, { code: 'ERR_INVALID_STATE' });
  await assert.rejects(text(readable), (error) => error === thrown);
}

Promise.all([
  testPipeToSyncEndSyncFailure(),
  testPipeToSyncEndSyncFailureDoesNotFailWriter(),
  testPipeToSyncFailOnIncompleteClose(),
  testPipeToSyncNoEndSync(),
  testPipeToSyncPreventFail(),
  testPipeToSyncPreventClose(),
  testFailThrowingDoesNotMaskError(),
]).then(common.mustCall());
