// Flags: --experimental-stream-iter
'use strict';

// Iterator results created by the stream/iter iterators themselves do not
// inherit from Object.prototype, so prototype pollution cannot affect them.
// (Iterators implemented as async generators, such as the one returned by
// pull(), return ordinary iterator results created by the engine.)

const common = require('../common');
const assert = require('assert');
const { inspect } = require('util');
const {
  broadcast,
  from,
  push,
  share,
  shareSync,
} = require('stream/iter');

function assertResult(result, done) {
  assert.strictEqual(result instanceof Object, false);
  assert.deepStrictEqual(Object.keys(result), ['done', 'value']);
  assert.strictEqual(result.done, done);
  assert.match(inspect(result), /^IterResult \{ done: (true|false), value: /);
}

async function testAsyncIterators() {
  const sources = {
    'push()': () => {
      const { writer, readable } = push();
      writer.writeSync('a');
      writer.endSync();
      return readable;
    },
    'share()': () => share(from('a')).pull(),
    'broadcast()': () => {
      const { writer, broadcast: bc } = broadcast();
      const consumer = bc.push();
      writer.writeSync('a');
      writer.endSync();
      return consumer;
    },
  };
  for (const create of Object.values(sources)) {
    const iterator = create()[Symbol.asyncIterator]();
    assertResult(await iterator.next(), false);
    assertResult(await iterator.next(), true);
  }
}

function testShareSync() {
  const iterator = shareSync(['a']).pull()[Symbol.iterator]();
  assertResult(iterator.next(), false);
  assertResult(iterator.next(), true);
}

async function testPollutedThen() {
  // Resolving an async next() with an object looks up `then`. Results must
  // not pick it up from a polluted Object.prototype.
  const { writer, readable } = push();
  writer.writeSync('ab');
  writer.endSync();
  const iterator = readable[Symbol.asyncIterator]();
  Object.prototype.then = common.mustNotCall('Object.prototype.then');
  try {
    const first = await iterator.next();
    assert.strictEqual(first.done, false);
    assert.strictEqual(new TextDecoder().decode(first.value[0]), 'ab');
    assert.strictEqual((await iterator.next()).done, true);
  } finally {
    delete Object.prototype.then;
  }
}

(async () => {
  await testAsyncIterators();
  testShareSync();
  await testPollutedThen();
})().then(common.mustCall());
