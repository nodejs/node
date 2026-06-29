'use strict';
const common = require('../common');

const { MessageChannel } = require('worker_threads');
const { createHook } = require('async_hooks');
const { strictEqual } = require('assert');

const handles = [];

createHook({
  init(asyncId, type, triggerAsyncId, resource) {
    if (type === 'MESSAGEPORT') {
      handles.push(resource);
    }
  }
}).enable();

const { port1, port2 } = new MessageChannel();
strictEqual(handles[0], port1);
strictEqual(handles[1], port2);

strictEqual(handles[0].hasRef(), false);
strictEqual(handles[1].hasRef(), false);

port1.unref();
strictEqual(handles[0].hasRef(), false);

port1.ref();
strictEqual(handles[0].hasRef(), true);

port1.unref();
strictEqual(handles[0].hasRef(), false);

port1.on('message', () => {});
strictEqual(handles[0].hasRef(), true);

port2.unref();
strictEqual(handles[1].hasRef(), false);

port2.ref();
strictEqual(handles[1].hasRef(), true);

port2.unref();
strictEqual(handles[1].hasRef(), false);

port2.on('message', () => {});
strictEqual(handles[0].hasRef(), true);

port1.on('close', common.mustCall(() => {
  strictEqual(handles[0].hasRef(), false);
  strictEqual(handles[1].hasRef(), false);
}));

port2.close();

strictEqual(handles[0].hasRef(), true);
strictEqual(handles[1].hasRef(), true);
