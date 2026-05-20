'use strict';
const common = require('../common');

const { MessageChannel } = require('worker_threads');
const { createHook } = require('async_hooks');
const assert = require('assert');

const handles = [];

createHook({
  init(asyncId, type, triggerAsyncId, resource) {
    if (type === 'MESSAGEPORT') {
      handles.push(resource);
    }
  }
}).enable();

const { port1, port2 } = new MessageChannel();
assert.strictEqual(handles[0], port1);
assert.strictEqual(handles[1], port2);

assert.strictEqual(handles[0].hasRef(), false);
assert.strictEqual(handles[1].hasRef(), false);

port1.unref();
assert.strictEqual(handles[0].hasRef(), false);

port1.ref();
assert.strictEqual(handles[0].hasRef(), true);

port1.unref();
assert.strictEqual(handles[0].hasRef(), false);

port1.on('message', () => {});
assert.strictEqual(handles[0].hasRef(), true);

port2.unref();
assert.strictEqual(handles[1].hasRef(), false);

port2.ref();
assert.strictEqual(handles[1].hasRef(), true);

port2.unref();
assert.strictEqual(handles[1].hasRef(), false);

port2.on('message', () => {});
assert.strictEqual(handles[0].hasRef(), true);

port1.on('close', common.mustCall(() => {
  assert.strictEqual(handles[0].hasRef(), false);
  assert.strictEqual(handles[1].hasRef(), false);
}));

port2.close();

assert.strictEqual(handles[0].hasRef(), true);
assert.strictEqual(handles[1].hasRef(), true);
