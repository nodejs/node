'use strict';

const common = require('../common');
const assert = require('assert');
const { pull, pullSync, pipeTo, pipeToSync, from, fromSync, bytesSync,
        text } = require('stream/new');

// =============================================================================
// pullSync() tests
// =============================================================================

async function testPullSyncIdentity() {
  // No transforms - just pass through
  const source = fromSync('hello');
  const result = pullSync(source);
  const data = bytesSync(result);
  assert.deepStrictEqual(data, new TextEncoder().encode('hello'));
}

async function testPullSyncStatelessTransform() {
  const source = fromSync('abc');
  const upper = (chunks) => {
    if (chunks === null) return null;
    return chunks.map((c) => {
      const str = new TextDecoder().decode(c);
      return new TextEncoder().encode(str.toUpperCase());
    });
  };
  const result = pullSync(source, upper);
  const data = bytesSync(result);
  assert.deepStrictEqual(data, new TextEncoder().encode('ABC'));
}

async function testPullSyncStatefulTransform() {
  const source = fromSync('data');
  const stateful = {
    transform: function*(source) {
      for (const chunks of source) {
        if (chunks === null) {
          // Flush: emit trailer
          yield new TextEncoder().encode('-END');
          continue;
        }
        for (const chunk of chunks) {
          yield chunk;
        }
      }
    },
  };
  const result = pullSync(source, stateful);
  const data = new TextDecoder().decode(bytesSync(result));
  assert.strictEqual(data, 'data-END');
}

async function testPullSyncChainedTransforms() {
  const source = fromSync('hello');
  const addExcl = (chunks) => {
    if (chunks === null) return null;
    return [...chunks, new TextEncoder().encode('!')];
  };
  const addQ = (chunks) => {
    if (chunks === null) return null;
    return [...chunks, new TextEncoder().encode('?')];
  };
  const result = pullSync(source, addExcl, addQ);
  const data = new TextDecoder().decode(bytesSync(result));
  assert.strictEqual(data, 'hello!?');
}

// =============================================================================
// pull() tests (async)
// =============================================================================

async function testPullIdentity() {
  const source = from('hello-async');
  const result = pull(source);
  const data = await text(result);
  assert.strictEqual(data, 'hello-async');
}

async function testPullStatelessTransform() {
  const source = from('abc');
  const upper = (chunks) => {
    if (chunks === null) return null;
    return chunks.map((c) => {
      const str = new TextDecoder().decode(c);
      return new TextEncoder().encode(str.toUpperCase());
    });
  };
  const result = pull(source, upper);
  const data = await text(result);
  assert.strictEqual(data, 'ABC');
}

async function testPullStatefulTransform() {
  const source = from('data');
  const stateful = {
    transform: async function*(source) {
      for await (const chunks of source) {
        if (chunks === null) {
          yield new TextEncoder().encode('-ASYNC-END');
          continue;
        }
        for (const chunk of chunks) {
          yield chunk;
        }
      }
    },
  };
  const result = pull(source, stateful);
  const data = await text(result);
  assert.strictEqual(data, 'data-ASYNC-END');
}

async function testPullWithAbortSignal() {
  const ac = new AbortController();
  ac.abort();

  async function* gen() {
    yield [new Uint8Array([1])];
  }

  const result = pull(gen(), { signal: ac.signal });
  await assert.rejects(
    async () => {
      // eslint-disable-next-line no-unused-vars
      for await (const _ of result) {
        assert.fail('Should not reach here');
      }
    },
    (err) => err.name === 'AbortError',
  );
}

async function testPullChainedTransforms() {
  const source = from('hello');
  const transforms = [
    (chunks) => {
      if (chunks === null) return null;
      return [...chunks, new TextEncoder().encode('!')];
    },
    (chunks) => {
      if (chunks === null) return null;
      return [...chunks, new TextEncoder().encode('?')];
    },
  ];
  const result = pull(source, ...transforms);
  const data = await text(result);
  assert.strictEqual(data, 'hello!?');
}

// =============================================================================
// pipeTo() / pipeToSync() tests
// =============================================================================

async function testPipeToSync() {
  const source = fromSync('pipe-data');
  const written = [];
  const writer = {
    write(chunk) { written.push(chunk); },
    end() { return written.length; },
    abort() {},
  };

  const totalBytes = pipeToSync(source, writer);
  assert.ok(totalBytes > 0);
  assert.ok(written.length > 0);
  const result = new TextDecoder().decode(
    new Uint8Array(written.reduce((acc, c) => [...acc, ...c], [])));
  assert.strictEqual(result, 'pipe-data');
}

async function testPipeTo() {
  const source = from('async-pipe-data');
  const written = [];
  const writer = {
    async write(chunk) { written.push(chunk); },
    async end() { return written.length; },
    async abort() {},
  };

  const totalBytes = await pipeTo(source, writer);
  assert.ok(totalBytes > 0);
  assert.ok(written.length > 0);
}

async function testPipeToPreventClose() {
  const source = from('data');
  let endCalled = false;
  const writer = {
    async write() {},
    async end() { endCalled = true; },
    async abort() {},
  };

  await pipeTo(source, writer, { preventClose: true });
  assert.strictEqual(endCalled, false);
}

Promise.all([
  testPullSyncIdentity(),
  testPullSyncStatelessTransform(),
  testPullSyncStatefulTransform(),
  testPullSyncChainedTransforms(),
  testPullIdentity(),
  testPullStatelessTransform(),
  testPullStatefulTransform(),
  testPullWithAbortSignal(),
  testPullChainedTransforms(),
  testPipeToSync(),
  testPipeTo(),
  testPipeToPreventClose(),
]).then(common.mustCall());
