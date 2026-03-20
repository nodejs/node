'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

{
  const channel = dc.syncIteratorTracingChannel('test');

  assert.strictEqual(channel.yield.name, 'tracing:test:syncYield');
  assert.strictEqual(channel.return.name, 'tracing:test:return');
  assert.strictEqual(channel.error.name, 'tracing:test:error');

  function* values() {
    yield 1;
    return 2;
  }

  const wrapped = channel.wrap(values(), {
    context: { type: 'sync' },
  });

  channel.subscribe({
    yield: common.mustCall((context) => {
      assert.strictEqual(context.type, 'sync');
      assert.strictEqual(context.iterator.method, 'next');
      assert.deepStrictEqual(context.iterator.args, []);
      assert.strictEqual(context.iterator.result.value, 1);
      assert.strictEqual(context.iterator.result.done, false);
      assert.strictEqual('step' in context.iterator, false);
      assert.strictEqual('yieldIndex' in context.iterator, false);
      assert.strictEqual('kind' in context.iterator, false);
      assert.strictEqual('done' in context.iterator, false);
    }),
    return: common.mustCall((context) => {
      assert.strictEqual(context.type, 'sync');
      assert.strictEqual(context.iterator.method, 'next');
      assert.deepStrictEqual(context.iterator.args, []);
      assert.strictEqual(context.iterator.result.value, 2);
      assert.strictEqual(context.iterator.result.done, true);
    }),
    error: common.mustNotCall(),
  });

  assert.deepStrictEqual(wrapped.next(), { value: 1, done: false });
  assert.deepStrictEqual(wrapped.next(), { value: 2, done: true });
}

{
  const channel = dc.asyncIteratorTracingChannel('async');

  assert.strictEqual(channel.yield.name, 'tracing:async:asyncYield');
  assert.strictEqual(channel.return.name, 'tracing:async:return');
  assert.strictEqual(channel.error.name, 'tracing:async:error');

  const expectedError = new Error('boom');

  async function* values() {
    yield 1;
    throw expectedError;
  }

  const wrapped = channel.wrap(values(), {
    context: { type: 'async' },
  });

  channel.subscribe({
    yield: common.mustCall((context) => {
      assert.strictEqual(context.type, 'async');
      assert.strictEqual(context.iterator.method, 'next');
      assert.strictEqual(context.iterator.result.value, 1);
      assert.strictEqual(context.iterator.result.done, false);
    }),
    return: common.mustNotCall(),
    error: common.mustCall((context) => {
      assert.strictEqual(context.type, 'async');
      assert.strictEqual(context.iterator.method, 'next');
      assert.strictEqual(context.iterator.error, expectedError);
    }),
  });

  (async () => {
    const result = await wrapped.next();
    assert.deepStrictEqual(result, { value: 1, done: false });
    await assert.rejects(wrapped.next(), (err) => {
      assert.strictEqual(err, expectedError);
      return true;
    });
  })().then(common.mustCall());
}
