// Flags: --experimental-stream-iter
'use strict';

// Edge case tests for pipeToSync: endSync fallback, preventFail.

const common = require('../common');
const assert = require('assert');
const { pipeToSync, fromSync } = require('stream/iter');

// pipeToSync endSync returns negative → falls back to end()
async function testPipeToSyncEndSyncFallback() {
  let endCalled = false;
  const writer = {
    writeSync() { return true; },
    endSync() { return -1; }, // Negative → triggers end() fallback
    end() { endCalled = true; },
  };
  pipeToSync(fromSync('data'), writer);
  assert.strictEqual(endCalled, true);
}

// pipeToSync endSync missing → falls back to end()
async function testPipeToSyncNoEndSync() {
  let endCalled = false;
  const writer = {
    writeSync() { return true; },
    end() { endCalled = true; },
  };
  pipeToSync(fromSync('data'), writer);
  assert.strictEqual(endCalled, true);
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
  testPipeToSyncEndSyncFallback(),
  testPipeToSyncNoEndSync(),
  testPipeToSyncPreventFail(),
  testPipeToSyncPreventClose(),
]).then(common.mustCall());
