// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const { pullSync, fromSync, bytesSync, tapSync } = require('stream/iter');

function testPullSyncIdentity() {
  // No transforms - just pass through
  const data = bytesSync(pullSync(fromSync('hello')));
  assert.deepStrictEqual(data, new TextEncoder().encode('hello'));
}

function testPullSyncStatelessTransform() {
  const upper = (chunks) => {
    if (chunks === null) return null;
    return chunks.map((c) => {
      const str = new TextDecoder().decode(c);
      return new TextEncoder().encode(str.toUpperCase());
    });
  };
  const data = bytesSync(pullSync(fromSync('abc'), upper));
  assert.deepStrictEqual(data, new TextEncoder().encode('ABC'));
}

function testPullSyncStatefulTransform() {
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

function testPullSyncChainedTransforms() {
  const addExcl = (chunks) => {
    if (chunks === null) return null;
    return [...chunks, new TextEncoder().encode('!')];
  };
  const addQ = (chunks) => {
    if (chunks === null) return null;
    return [...chunks, new TextEncoder().encode('?')];
  };
  const result = pullSync(fromSync('hello'), addExcl, addQ);
  const data = new TextDecoder().decode(bytesSync(result));
  assert.strictEqual(data, 'hello!?');
}

// PullSync source error propagates
function testPullSyncSourceError() {
  function* failingSource() {
    yield [new TextEncoder().encode('a')];
    throw new Error('sync source boom');
  }
  assert.throws(() => {
    // eslint-disable-next-line no-unused-vars
    for (const _ of pullSync(failingSource())) { /* consume */ }
  }, { message: 'sync source boom' });
}

// PullSync with empty source
function testPullSyncEmptySource() {
  function* empty() {}
  const result = bytesSync(pullSync(empty()));
  assert.strictEqual(result.length, 0);
}

// TapSync callback error propagates
function testTapSyncCallbackError() {
  const badTap = tapSync(() => { throw new Error('tapSync boom'); });
  assert.throws(() => {
    // eslint-disable-next-line no-unused-vars
    for (const _ of pullSync(fromSync('hello'), badTap)) { /* consume */ }
  }, { message: 'tapSync boom' });
}

// Stateless transform error propagates
function testPullSyncStatelessTransformError() {
  const badTransform = (chunks) => {
    if (chunks === null) return null;
    throw new Error('stateless transform boom');
  };
  assert.throws(() => {
    // eslint-disable-next-line no-unused-vars
    for (const _ of pullSync(fromSync('hello'), badTransform)) { /* consume */ }
  }, { message: 'stateless transform boom' });
}

// Stateful transform error propagates
function testPullSyncStatefulTransformError() {
  const badStateful = {
    transform: function*(source) { // eslint-disable-line require-yield
      for (const chunks of source) {
        if (chunks === null) continue;
        throw new Error('stateful transform boom');
      }
    },
  };
  assert.throws(() => {
    // eslint-disable-next-line no-unused-vars
    for (const _ of pullSync(fromSync('hello'), badStateful)) { /* consume */ }
  }, { message: 'stateful transform boom' });
}

// Stateless transform flush emitting data
function testPullSyncStatelessTransformFlush() {
  const withTrailer = (chunks) => {
    if (chunks === null) {
      // Flush: emit trailing data
      return [new TextEncoder().encode('-TRAILER')];
    }
    return chunks;
  };
  const data = new TextDecoder().decode(bytesSync(pullSync(fromSync('data'), withTrailer)));
  assert.strictEqual(data, 'data-TRAILER');
}

// Stateless transform flush error propagates
function testPullSyncStatelessTransformFlushError() {
  const badFlush = (chunks) => {
    if (chunks === null) {
      throw new Error('flush boom');
    }
    return chunks;
  };
  assert.throws(() => {
    // eslint-disable-next-line no-unused-vars
    for (const _ of pullSync(fromSync('hello'), badFlush)) { /* consume */ }
  }, { message: 'flush boom' });
}

// Empty source result is a Uint8Array
function testPullSyncEmptySourceType() {
  function* empty() {}
  const result = bytesSync(pullSync(empty()));
  assert.ok(result instanceof Uint8Array);
  assert.strictEqual(result.byteLength, 0);
}

// Invalid transform argument
function testPullSyncInvalidTransform() {
  assert.throws(
    () => { for (const _ of pullSync(fromSync('x'), 42)) { /* consume */ } },  // eslint-disable-line no-unused-vars
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  assert.throws(
    () => { for (const _ of pullSync(fromSync('x'), null)) { /* consume */ } },  // eslint-disable-line no-unused-vars
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
}

Promise.all([
  testPullSyncIdentity(),
  testPullSyncStatelessTransform(),
  testPullSyncStatefulTransform(),
  testPullSyncChainedTransforms(),
  testPullSyncSourceError(),
  testPullSyncEmptySource(),
  testPullSyncEmptySourceType(),
  testTapSyncCallbackError(),
  testPullSyncStatelessTransformError(),
  testPullSyncStatefulTransformError(),
  testPullSyncStatelessTransformFlush(),
  testPullSyncStatelessTransformFlushError(),
  testPullSyncInvalidTransform(),
]).then(common.mustCall());
