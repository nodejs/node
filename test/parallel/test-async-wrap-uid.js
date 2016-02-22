'use strict';

require('../common');
const fs = require('fs');
const assert = require('assert');
const async_wrap = process.binding('async_wrap');

const storage = new Map();
async_wrap.setupHooks({ init, pre, post, destroy });
async_wrap.enable();

function init(uid) {
  storage.set(uid, {
    init: true,
    pre: false,
    post: false,
    destroy: false
  });
}

function pre(uid) {
  storage.get(uid).pre = true;
}

function post(uid) {
  storage.get(uid).post = true;
}

function destroy(uid) {
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
