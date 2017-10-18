'use strict';

// Regression test for https://github.com/nodejs/node/issues/13237

const common = require('../common');
const assert = require('assert');

const async_hooks = require('async_hooks');

const seenEvents = [];

let asyncId;
const p = new Promise((resolve) => resolve(1));
p.then((res) => {
  asyncId = async_hooks.executionAsyncId();
  seenEvents.push('then');
});

const hooks = async_hooks.createHook({
  init: common.mustNotCall(),

  before: (id) => {
    assert.ok(id > 1);
    seenEvents.push(`before${id}`);
  },

  after: (id) => {
    assert.ok(id > 1);
    if (id === asyncId) {
      seenEvents.push(`after${id}`);
      hooks.disable();
    }
  }
});

setImmediate(() => {
  assert.deepStrictEqual(seenEvents.slice(-3),
                         [`before${asyncId}`, 'then', `after${asyncId}`]);
});

hooks.enable(); // After `setImmediate` in order to not catch its init event.
