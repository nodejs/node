/* global port */
'use strict';
const common = require('../common');
const assert = require('assert');
const vm = require('vm');
const {
  MessagePort, MessageChannel, moveMessagePortToContext
} = require('worker_threads');

const context = vm.createContext();
const { port1, port2 } = new MessageChannel();
context.port = moveMessagePortToContext(port1, context);
context.global = context;
Object.assign(context, {
  global: context,
  assert,
  MessagePort,
  MessageChannel
});

vm.runInContext('(' + function() {
  {
    assert(port.postMessage instanceof Function);
    assert(port.constructor instanceof Function);
    for (let obj = port; obj !== null; obj = Object.getPrototypeOf(obj)) {
      for (const key of Object.getOwnPropertyNames(obj)) {
        if (typeof obj[key] === 'object' && obj[key] !== null) {
          assert(obj[key] instanceof Object);
        } else if (typeof obj[key] === 'function') {
          assert(obj[key] instanceof Function);
        }
      }
    }

    assert(!(port instanceof MessagePort));
    assert.strictEqual(port.onmessage, undefined);
    port.onmessage = function({ data }) {
      assert(data instanceof Object);
      port.postMessage(data);
    };
    port.start();
  }

  {
    let threw = false;
    try {
      port.postMessage(global);
    } catch (e) {
      assert.strictEqual(e.constructor.name, 'DOMException');
      assert(e instanceof Object);
      assert(e instanceof Error);
      threw = true;
    }
    assert(threw);
  }
} + ')()', context);

port2.on('message', common.mustCall((msg) => {
  assert(msg instanceof Object);
  port2.close();
}));
port2.postMessage({});
