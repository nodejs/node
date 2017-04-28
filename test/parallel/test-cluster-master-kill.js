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

  // keep the worker alive
  const http = require('http');
  http.Server().listen(0, '127.0.0.1');

} else if (process.argv[2] === 'cluster') {

  const worker = cluster.fork();

  // send PID info to testcase process
  process.send({
    pid: worker.process.pid
  });

  // terminate the cluster process
  worker.once('listening', common.mustCall(() => {
    setTimeout(() => {
      process.exit(0);
    }, 1000);
  }));

} else {

  // This is the testcase
  const fork = require('child_process').fork;

  // Spawn a cluster process
  const master = fork(process.argv[1], ['cluster']);

  // get pid info
  let pid = null;
  master.once('message', (data) => {
    pid = data.pid;
  });

  // When master is dead
  let alive = true;
  master.on('exit', common.mustCall((code) => {

    // make sure that the master died on purpose
    assert.strictEqual(code, 0);

    // check worker process status
    const pollWorker = () => {
      alive = common.isAlive(pid);
      if (alive) {
        setTimeout(pollWorker, 50);
      }
    };
    // Loop indefinitely until worker exit.
    pollWorker();
  }));

  process.once('exit', () => {
    assert.strictEqual(typeof pid, 'number', 'did not get worker pid info');
    assert.strictEqual(alive, false, 'worker was alive after master died');
  });

}
