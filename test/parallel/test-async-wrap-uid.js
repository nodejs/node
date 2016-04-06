'use strict';

require('../common');
const fs = require('fs');
const assert = require('assert');
const async_wrap = process.binding('async_wrap');

const storage = new Map();
async_wrap.setupHooks({ init, pre, post, destroy });
async_wrap.enable();

function init(uid) {
  assert.notStrictEqual(async_wrap.getCurrentAsyncId(), uid);
  storage.set(uid, {
    init: true,
    pre: false,
    post: false,
    destroy: false
  });
}

function pre(uid) {
  assert.strictEqual(async_wrap.getCurrentAsyncId(), uid);
  storage.get(uid).pre = true;
}

function post(uid) {
  assert.strictEqual(async_wrap.getCurrentAsyncId(), uid);
  storage.get(uid).post = true;
}

function destroy(uid) {
  assert.notStrictEqual(async_wrap.getCurrentAsyncId(), uid);
  storage.get(uid).destroy = true;
}

fs.access(__filename, function(err) {
  assert.ifError(err);
});

fs.access(__filename, function(err) {
  assert.ifError(err);
});

async_wrap.disable();

process.once('exit', function() {
  assert.strictEqual(storage.size, 2);

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

// verify each call to next ID produces an increasing uid.
var nextId = async_wrap.getNextAsyncId();
var nextId2 = async_wrap.getNextAsyncId();
assert.strictEqual(nextId + 1, nextId2);
