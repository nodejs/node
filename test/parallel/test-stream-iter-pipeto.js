// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const { pipeTo, pipeToSync, from, fromSync } = require('stream/iter');

async function testPipeToSync() {
  const written = [];
  const writer = {
    writeSync(chunk) { written.push(chunk); return true; },
    endSync() { return written.length; },
    fail() {},
  };

  const totalBytes = pipeToSync(fromSync('pipe-data'), writer);
  assert.strictEqual(totalBytes, 9); // 'pipe-data' = 9 UTF-8 bytes
  assert.ok(written.length > 0);
  const result = new TextDecoder().decode(
    new Uint8Array(written.reduce((acc, c) => [...acc, ...c], [])));
  assert.strictEqual(result, 'pipe-data');
}

async function testPipeTo() {
  const written = [];
  const writer = {
    async write(chunk) { written.push(chunk); },
    async end() { return written.length; },
    async fail() {},
  };

  const totalBytes = await pipeTo(from('async-pipe-data'), writer);
  assert.strictEqual(totalBytes, 15); // 'async-pipe-data' = 15 UTF-8 bytes
  assert.ok(written.length > 0);
}

async function testPipeToPreventClose() {
  let endCalled = false;
  const writer = {
    async write() {},
    async end() { endCalled = true; },
    async fail() {},
  };

  await pipeTo(from('data'), writer, { preventClose: true });
  assert.strictEqual(endCalled, false);
}

// PipeTo source error calls writer.fail()
async function testPipeToSourceError() {
  let failCalled = false;
  let failReason;
  const writer = {
    write() {},
    fail(reason) { failCalled = true; failReason = reason; },
  };
  async function* failingSource() {
    yield [new TextEncoder().encode('a')];
    throw new Error('pipe source boom');
  }
  await assert.rejects(
    () => pipeTo(failingSource(), writer),
    { message: 'pipe source boom' },
  );
  assert.strictEqual(failCalled, true);
  assert.strictEqual(failReason.message, 'pipe source boom');
}

// PipeToSync source error calls writer.fail()
async function testPipeToSyncSourceError() {
  let failCalled = false;
  const writer = {
    writeSync() { return true; },
    fail(reason) { failCalled = true; },
  };
  function* failingSource() {
    yield [new TextEncoder().encode('a')];
    throw new Error('sync pipe boom');
  }
  assert.throws(
    () => pipeToSync(failingSource(), writer),
    { message: 'sync pipe boom' },
  );
  assert.strictEqual(failCalled, true);
}

// PipeTo with AbortSignal
async function testPipeToWithSignal() {
  const ac = new AbortController();
  const chunks = [];
  const writer = {
    write(chunk) { chunks.push(chunk); },
  };
  async function* slowSource() {
    yield [new TextEncoder().encode('a')];
    await new Promise((r) => setTimeout(r, 50));
    yield [new TextEncoder().encode('b')];
  }
  ac.abort();
  await assert.rejects(
    () => pipeTo(slowSource(), writer, { signal: ac.signal }),
    { name: 'AbortError' },
  );
}

// PipeTo with transforms
async function testPipeToWithTransforms() {
  const chunks = [];
  const writer = {
    write(chunk) { chunks.push(new TextDecoder().decode(chunk)); },
  };
  const upper = (batch) => {
    if (batch === null) return null;
    return batch.map((c) => {
      const out = new Uint8Array(c);
      for (let i = 0; i < out.length; i++)
        out[i] -= (out[i] >= 97 && out[i] <= 122) * 32;
      return out;
    });
  };
  await pipeTo(from('hello'), upper, writer);
  assert.strictEqual(chunks.join(''), 'HELLO');
}

// PipeToSync with transforms
async function testPipeToSyncWithTransforms() {
  const chunks = [];
  const writer = {
    writeSync(chunk) { chunks.push(new TextDecoder().decode(chunk)); return true; },
  };
  const upper = (batch) => {
    if (batch === null) return null;
    return batch.map((c) => {
      const out = new Uint8Array(c);
      for (let i = 0; i < out.length; i++)
        out[i] -= (out[i] >= 97 && out[i] <= 122) * 32;
      return out;
    });
  };
  pipeToSync(fromSync('hello'), upper, writer);
  assert.strictEqual(chunks.join(''), 'HELLO');
}

// PipeTo with writev writer
async function testPipeToWithWritevWriter() {
  const allChunks = [];
  const writer = {
    write(chunk) { allChunks.push(chunk); },
    writev(chunks) { allChunks.push(...chunks); },
  };
  await pipeTo(from('hello world'), writer);
  assert.strictEqual(allChunks.length > 0, true);
}

// PipeTo with writeSync/writevSync fallback
async function testPipeToSyncFallback() {
  const chunks = [];
  const writer = {
    writeSync(chunk) { chunks.push(chunk); return true; },
    write(chunk) { chunks.push(chunk); },
  };
  await pipeTo(from('hello'), writer);
  assert.strictEqual(chunks.length > 0, true);
}

// PipeTo preventFail option
async function testPipeToPreventFail() {
  let failCalled = false;
  const writer = {
    write() {},
    fail() { failCalled = true; },
  };
  // eslint-disable-next-line require-yield
  async function* failingSource() {
    throw new Error('boom');
  }
  await assert.rejects(
    () => pipeTo(failingSource(), writer, { preventFail: true }),
    { message: 'boom' },
  );
  assert.strictEqual(failCalled, false);
}

// PipeToSync preventClose option
async function testPipeToSyncPreventClose() {
  let endCalled = false;
  const writer = {
    writeSync() { return true; },
    endSync() { endCalled = true; return 0; },
  };
  pipeToSync(fromSync('hello'), writer, { preventClose: true });
  assert.strictEqual(endCalled, false);
}

// Regression test: pipeTo should work with a minimal writer that only
// implements write(). end(), fail(), and all *Sync methods are optional.
async function testPipeToMinimalWriter() {
  const chunks = [];
  const minimalWriter = {
    write(chunk) {
      chunks.push(chunk);
    },
  };

  await pipeTo(from('minimal'), minimalWriter);
  assert.strictEqual(chunks.length > 0, true);
}

async function testPipeToSyncMinimalWriter() {
  const chunks = [];
  const minimalWriter = {
    writeSync(chunk) {
      chunks.push(chunk);
      return true;
    },
  };

  pipeToSync(fromSync('minimal-sync'), minimalWriter);
  assert.strictEqual(chunks.length > 0, true);
}

Promise.all([
  testPipeToSync(),
  testPipeTo(),
  testPipeToPreventClose(),
  testPipeToSourceError(),
  testPipeToSyncSourceError(),
  testPipeToWithSignal(),
  testPipeToWithTransforms(),
  testPipeToSyncWithTransforms(),
  testPipeToWithWritevWriter(),
  testPipeToSyncFallback(),
  testPipeToPreventFail(),
  testPipeToSyncPreventClose(),
  testPipeToMinimalWriter(),
  testPipeToSyncMinimalWriter(),
]).then(common.mustCall());
