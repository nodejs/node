'use strict';

const common = require('../common');
const assert = require('assert');
const EventEmitter = require('events');

const ee = new EventEmitter();

{
  ee.when('foo')
    .then(common.mustCall((context) => {
      assert.strictEqual(context.emitter, ee);
      assert.deepStrictEqual(context.args, [1, 2, 3]);
    }))
    .catch(common.mustNotCall());
  assert.strictEqual(ee.listenerCount('foo'), 1);
  ee.emit('foo', 1, 2, 3);
  assert.strictEqual(ee.listenerCount('foo'), 0);
}

{
  let a = 1;
  ee.when('foo')
    .then(common.mustCall(() => {
      assert.strictEqual(a, 2);
    }))
    .catch(common.mustNotCall());

  ee.when('foo', { prepend: true })
    .then(common.mustCall(() => {
      assert.strictEqual(a++, 1);
    }))
    .catch(common.mustNotCall());

  ee.emit('foo');
}

{
  ee.when('foo')
    .then(common.mustCall(() => {
      throw new Error('foo');
    }))
    .catch(common.mustCall((err) => {
      assert.strictEqual(err.message, 'foo');
    }));
  assert.strictEqual(ee.listenerCount('foo'), 1);
  ee.emit('foo');
  assert.strictEqual(ee.listenerCount('foo'), 0);
}

{
  ee.removeAllListeners();
  ee.when('foo')
    .then(common.mustNotCall())
    .catch(common.mustCall((reason) => {
      assert.strictEqual(reason, 'canceled');
    }));
  ee.removeAllListeners();
}

{
  ee.removeAllListeners();
  assert.strictEqual(ee.listenerCount(0), 0);
  const promise = ee.when('foo');
  promise.then(common.mustNotCall())
         .catch(common.mustCall((reason) => {
           assert.strictEqual(reason, 'canceled');
         }));
  const fn = ee.listeners('foo')[0];
  assert.strictEqual(fn.name, 'promise for \'foo\'');
  assert.strictEqual(fn.promise, promise);
  ee.removeListener('foo', fn);
}
