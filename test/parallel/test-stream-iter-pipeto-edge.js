// Flags: --experimental-stream-iter
'use strict';

// Edge case tests for pipeToSync close and failure behavior.

const common = require('../common');
const assert = require('assert');
const { pipeToSync, fromSync } = require('stream/iter');

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

Promise.all([
  testPipeToSyncEndSyncFailure(),
  testPipeToSyncNoEndSync(),
  testPipeToSyncPreventFail(),
  testPipeToSyncPreventClose(),
]).then(common.mustCall());
