// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

if (cluster.isWorker) {
  const http = require('http');
  http.Server(() => {

  }).listen(0, '127.0.0.1');
  const worker = cluster.worker;
  assert.strictEqual(worker.exitedAfterDisconnect, worker.suicide);

  cluster.worker.on('disconnect', common.mustCall(() => {
    assert.strictEqual(cluster.worker.exitedAfterDisconnect,
                       cluster.worker.suicide);
    process.exit(42);
  }));

} else if (cluster.isMaster) {

  const checks = {
    cluster: {
      emitDisconnect: false,
      emitExit: false,
      callback: false
    },
    worker: {
      emitDisconnect: false,
      emitDisconnectInsideWorker: false,
      emitExit: false,
      state: false,
      voluntaryMode: false,
      died: false
    }
  };

  // start worker
  const worker = cluster.fork();

  // Disconnect worker when it is ready
  worker.once('listening', common.mustCall(() => {
    const w = worker.disconnect();
    assert.strictEqual(worker, w, `${worker.id} did not return a reference`);
  }));

  // Check cluster events
  cluster.once('disconnect', common.mustCall(() => {
    checks.cluster.emitDisconnect = true;
  }));
  cluster.once('exit', common.mustCall(() => {
    checks.cluster.emitExit = true;
  }));

  // Check worker events and properties
  worker.once('disconnect', common.mustCall(() => {
    checks.worker.emitDisconnect = true;
    checks.worker.voluntaryMode = worker.exitedAfterDisconnect;
    checks.worker.state = worker.state;
  }));

  // Check that the worker died
  worker.once('exit', common.mustCall((code) => {
    checks.worker.emitExit = true;
    checks.worker.died = !common.isAlive(worker.process.pid);
    checks.worker.emitDisconnectInsideWorker = code === 42;
  }));

  process.once('exit', () => {

    const w = checks.worker;
    const c = checks.cluster;

    // events
    assert.ok(w.emitDisconnect, 'Disconnect event did not emit');
    assert.ok(w.emitDisconnectInsideWorker,
              'Disconnect event did not emit inside worker');
    assert.ok(c.emitDisconnect, 'Disconnect event did not emit');
    assert.ok(w.emitExit, 'Exit event did not emit');
    assert.ok(c.emitExit, 'Exit event did not emit');

    // flags
    assert.strictEqual(w.state, 'disconnected');
    assert.strictEqual(w.voluntaryMode, true);

    // is process alive
    assert.ok(w.died, 'The worker did not die');
  });
}
