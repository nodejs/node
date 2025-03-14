// Flags: --expose-gc
'use strict';

const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

{
  const domexception = new DOMException('test');
  const w = new Worker(`
    const { parentPort } = require('worker_threads');
    parentPort.on('message', (event) => {
      if (event.type === 'error') {
        console.log('event', event.domexception);
        // parentPort.postMessage(event);
      }
    });
  `, { eval: true });


  w.on('message', common.mustCall(({ domexception: d }) => {
    assert.strictEqual(d.message, domexception.message);
    assert.strictEqual(d.name, domexception.name);
    assert.strictEqual(d.code, domexception.code);
    globalThis.gc();
    w.terminate();
  }));

  console.log('posting message');
  w.postMessage({ type: 'error', domexception });
  // This would be a race condition if the memory regions were overlapping
  // local.write('Hello ');
}
