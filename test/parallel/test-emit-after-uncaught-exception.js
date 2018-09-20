'use strict';

const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');

const id_obj = {};
let collect = true;

const hook = async_hooks.createHook({
  before(id) { if (collect) id_obj[id] = true; },
  after(id) { delete id_obj[id]; },
}).enable();

process.once('uncaughtException', common.mustCall((er) => {
  assert.strictEqual(er.message, 'bye');
  collect = false;
}));

setImmediate(common.mustCall(() => {
  process.nextTick(common.mustCall(() => {
    assert.strictEqual(Object.keys(id_obj).length, 0);
    hook.disable();
  }));

  // Create a stack of async ids that will need to be emitted in the case of
  // an uncaught exception.
  const ar1 = new async_hooks.AsyncResource('Mine');
  ar1.emitBefore();

  const ar2 = new async_hooks.AsyncResource('Mine');
  ar2.emitBefore();

  throw new Error('bye');

  // TODO(trevnorris): This test shows that the after() hooks are always called
  // correctly, but it doesn't solve where the emitDestroy() is missed because
  // of the uncaught exception. Simple solution is to always call emitDestroy()
  // before the emitAfter(), but how to codify this?
}));
