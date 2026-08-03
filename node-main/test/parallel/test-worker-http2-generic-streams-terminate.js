'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const { Duplex } = require('stream');
const { Worker, workerData } = require('worker_threads');

// Tests the interaction between terminating a Worker thread and running
// the native SetImmediate queue, which may attempt to perform multiple
// calls into JS even though one already terminates the Worker.

if (!workerData) {
  const counter = new Int32Array(new SharedArrayBuffer(4));
  const worker = new Worker(__filename, { workerData: { counter } });
  worker.on('exit', common.mustCall(() => {
    assert.strictEqual(counter[0], 1);
  }));
} else {
  const { counter } = workerData;

  // Start two HTTP/2 connections. This will trigger write()s call from inside
  // the SetImmediate queue.
  for (let i = 0; i < 2; i++) {
    http2.connect('http://localhost', {
      createConnection() {
        return new Duplex({
          write(chunk, enc, cb) {
            Atomics.add(counter, 0, 1);
            process.exit();
          },
          read() { }
        });
      }
    });
  }
}
