'use strict';

require('../common');
require('console');
const assert = require('assert');
const async_wrap = process.binding('async_wrap');
const async_wrap_module = require('async_wrap');

const storage = new Map();
async_wrap.setupHooks({ init, pre, post, destroy });
async_wrap.enable();

function init(uid) {
  assert.notStrictEqual(async_wrap_module.getCurrentAsyncId(), uid);
  storage.set(uid, {
    init: true,
    pre: false,
    post: false,
    destroy: false
  });
  // track uid on the this pointer to confirm the same object is being pased
  // to subsequent callbacks
  this.uid = uid;
}

function pre(uid) {
  assert.strictEqual(async_wrap_module.getCurrentAsyncId(), uid);
  assert.strictEqual(this.uid, uid);
  storage.get(uid).pre = true;
}

function post(uid) {
  assert.strictEqual(async_wrap_module.getCurrentAsyncId(), uid);
  assert.strictEqual(this.uid, uid);
  storage.get(uid).post = true;
}

function destroy(uid) {
  assert.notStrictEqual(async_wrap_module.getCurrentAsyncId(), uid);
  storage.get(uid).destroy = true;
}

function validateAsyncCallbacksDuringNextTickCallback() {
  const currentAsyncId = async_wrap_module.getCurrentAsyncId();
  assert.strictEqual(storage.get(currentAsyncId).init, true);
  assert.strictEqual(storage.get(currentAsyncId).pre, true);
  assert.strictEqual(storage.get(currentAsyncId).post, false);
  assert.strictEqual(storage.get(currentAsyncId).destroy, false);
}

let id1 = 0;
let id2 = 0;
let id3 = 0;
let id4 = 0;

process.nextTick(function tick1() {

  id1 = async_wrap_module.getCurrentAsyncId();
  assert.notStrictEqual(id1, 0);
  assert.strictEqual(storage.size, 1);
  validateAsyncCallbacksDuringNextTickCallback();

  process.nextTick(function tick2() {
    id2 = async_wrap_module.getCurrentAsyncId();
    assert.notStrictEqual(id2, 0);
    assert.notStrictEqual(id1, id2);
    validateAsyncCallbacksDuringNextTickCallback();

    process.nextTick(function tick3() {
      id3 = async_wrap_module.getCurrentAsyncId();
      assert.notStrictEqual(id3, 0);
      assert.notStrictEqual(id1, id3);
      assert.notStrictEqual(id2, id3);
      validateAsyncCallbacksDuringNextTickCallback();

      // async-wrap callbacks should be disabled when this is enqueued
      // so init shouldn't fire
      process.nextTick(function tick4() {
        // but currentAsyncId should still be set up
        id4 = async_wrap_module.getCurrentAsyncId();
        assert.notStrictEqual(id4, 0);
        assert.notStrictEqual(id1, id4);
        assert.notStrictEqual(id2, id4);
        assert.notStrictEqual(id3, id4);
        assert.strictEqual(!storage.get(id4), true);
        assert.strictEqual(storage.size, 3);
      });
    });

    async_wrap.disable();

    assert.strictEqual(storage.size, 3);
    assert.strictEqual(id2, async_wrap_module.getCurrentAsyncId());
  });

  assert.strictEqual(storage.size, 2);
  assert.strictEqual(id1, async_wrap_module.getCurrentAsyncId());
});

process.once('exit', function() {

  assert.strictEqual(storage.size, 3);

  for (const item of storage) {
    const uid = item[0];
    const value = item[1];
    assert.strictEqual(typeof uid, 'number');
    assert.deepStrictEqual(value, {
      init: true,
      pre: true,
      post: true,
      destroy: true
    });
  }
});
