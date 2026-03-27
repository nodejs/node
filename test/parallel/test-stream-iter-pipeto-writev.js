// Flags: --experimental-stream-iter
'use strict';

// Tests for pipeTo writev/writevSync paths and writeBatchAsyncFallback.

const common = require('../common');
const assert = require('assert');
const { pipeTo, pipeToSync } = require('stream/iter');

// Multi-chunk batch with writevSync (sync success path)
async function testWritevSyncSuccess() {
  const batches = [];
  const writer = {
    write(chunk) {},
    writevSync(chunks) { batches.push(chunks); return true; },
    writev(chunks) { batches.push(chunks); },
    writeSync(chunk) { return true; },
    endSync() { return 0; },
  };
  // Source that yields multi-chunk batches
  async function* source() {
    yield [new Uint8Array([1]), new Uint8Array([2]), new Uint8Array([3])];
    yield [new Uint8Array([4]), new Uint8Array([5])];
  }
  const total = await pipeTo(source(), writer);
  assert.ok(batches.length > 0);
  // writevSync was used for multi-chunk batches
  assert.ok(batches.some((b) => b.length > 1));
  assert.strictEqual(total, 5);
}

// Multi-chunk batch with writev async (no writevSync)
async function testWritevAsyncFallback() {
  const batches = [];
  const writer = {
    async writev(chunks) { batches.push(chunks); },
    async write(chunk) { batches.push([chunk]); },
    async end() {},
  };
  async function* source() {
    yield [new Uint8Array([1]), new Uint8Array([2]), new Uint8Array([3])];
  }
  await pipeTo(source(), writer);
  assert.ok(batches.length > 0);
  assert.ok(batches.some((b) => b.length > 1));
}

// writevSync returns false — falls through to async writev
async function testWritevSyncFails() {
  const asyncCalls = [];
  const writer = {
    write() {},
    writevSync() { return false; },
    async writev(chunks) { asyncCalls.push(chunks); },
    writeSync() { return true; },
    endSync() { return 0; },
  };
  async function* source() {
    yield [new Uint8Array([1]), new Uint8Array([2])];
  }
  await pipeTo(source(), writer);
  assert.strictEqual(asyncCalls.length, 1);
  assert.strictEqual(asyncCalls[0].length, 2);
}

// writeSync fails mid-batch — triggers writeBatchAsyncFallback
async function testWriteSyncFailsMidBatch() {
  const asyncWrites = [];
  const writer = {
    writeSync(chunk) {
      // Fail for chunk value 2 — always, including retries
      if (chunk[0] === 2) return false;
      return true;
    },
    async write(chunk) { asyncWrites.push(chunk); },
    async end() {},
  };
  // Single batch with 3 chunks
  async function* source() {
    yield [new Uint8Array([1]), new Uint8Array([2]), new Uint8Array([3])];
  }
  const total = await pipeTo(source(), writer);
  // Chunk 1: writeSync succeeds
  // Chunk 2: writeSync fails → writeBatchAsyncFallback → write() called
  // Chunk 3: writeBatchAsyncFallback retries writeSync → succeeds
  assert.ok(asyncWrites.length >= 1);
  assert.deepStrictEqual(asyncWrites[0], new Uint8Array([2]));
  assert.strictEqual(total, 3);
}

// writeSync always fails — all chunks go through async
async function testWriteSyncAlwaysFails() {
  const asyncWrites = [];
  const writer = {
    writeSync() { return false; },
    async write(chunk) { asyncWrites.push(chunk); },
    async end() {},
  };
  async function* source() {
    yield [new Uint8Array([10]), new Uint8Array([20])];
  }
  const total = await pipeTo(source(), writer);
  assert.strictEqual(asyncWrites.length, 2);
  assert.strictEqual(total, 2);
}

// pipeToSync with writevSync
async function testPipeToSyncWritev() {
  const batches = [];
  const writer = {
    writevSync(chunks) { batches.push(chunks); },
    writeSync(chunk) { return true; },
    endSync() { return 0; },
  };
  function* source() {
    yield [new Uint8Array([1]), new Uint8Array([2]), new Uint8Array([3])];
    yield [new Uint8Array([4])];
  }
  pipeToSync(source(), writer);
  // Multi-chunk batch should have used writevSync
  assert.ok(batches.some((b) => b.length > 1));
}

// pipeToSync with writer that has write() and writeSync() — writeSync preferred
async function testPipeToSyncWriteFallback() {
  const syncWrites = [];
  const writer = {
    writeSync(chunk) { syncWrites.push(chunk); return true; },
    write(chunk) { /* should not be called */ },
    endSync() { return 0; },
  };
  function* source() {
    yield [new Uint8Array([1]), new Uint8Array([2])];
  }
  pipeToSync(source(), writer);
  assert.strictEqual(syncWrites.length, 2);
}

Promise.all([
  testWritevSyncSuccess(),
  testWritevAsyncFallback(),
  testWritevSyncFails(),
  testWriteSyncFailsMidBatch(),
  testWriteSyncAlwaysFails(),
  testPipeToSyncWritev(),
  testPipeToSyncWriteFallback(),
]).then(common.mustCall());
