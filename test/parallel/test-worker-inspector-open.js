'use strict';

const common = require('../common');
const assert = require('assert');

common.skipIfInspectorDisabled();

const net = require('net');
const { Worker, isMainThread, parentPort } = require('worker_threads');
const inspector = require('inspector');
const url = require('url');

// This test ensures that inspector.open(), inspector.close(), inspector.url()
// work inside Worker.
function ping(port, callback) {
  net.connect(port)
    .on('connect', function() { close(this); })
    .on('error', function(err) { close(this, err); });

  function close(self, err) {
    self.end();
    self.on('close', () => callback(err));
  }
}

function runMainThread() {
  const worker = new Worker(__filename);

  worker.on('message', common.mustCall(({ ws }) => {
    ping(url.parse(ws).port, common.mustCall((err) => {
      assert.ifError(err);
      worker.postMessage({ done: true });
    }));
  }));
}

function runChildWorkerThread() {
  inspector.open(0);
  assert(inspector.url());
  inspector.close();
  assert(!inspector.url());

  inspector.open(0);

  parentPort.on('message', common.mustCall(({ done }) => {
    if (done) {
      parentPort.close();
    }
  }));

  parentPort.postMessage({
    ws: inspector.url()
  });
}

if (isMainThread) {
  runMainThread();
} else {
  runChildWorkerThread();
}
