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
// test-cluster-worker-constructor.js
// validates correct behavior of the cluster.Worker constructor

require('../common');
const assert = require('assert');
const cluster = require('cluster');
let worker;

worker = new cluster.Worker();
assert.strictEqual(worker.exitedAfterDisconnect, undefined);
assert.strictEqual(worker.state, 'none');
assert.strictEqual(worker.id, 0);
assert.strictEqual(worker.process, undefined);

worker = new cluster.Worker({
  id: 3,
  state: 'online',
  process: process
});
assert.strictEqual(worker.exitedAfterDisconnect, undefined);
assert.strictEqual(worker.state, 'online');
assert.strictEqual(worker.id, 3);
assert.strictEqual(worker.process, process);

worker = cluster.Worker.call({}, { id: 5 });
assert(worker instanceof cluster.Worker);
assert.strictEqual(worker.id, 5);
