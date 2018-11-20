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

// This test asserts the semantics of dgram::socket.bind({ exclusive })
// when called from a cluster.Worker

const assert = require('assert');
const cluster = require('cluster');
const dgram = require('dgram');
const BYE = 'bye';
const WORKER2_NAME = 'wrker2';

if (cluster.isMaster) {
  const worker1 = cluster.fork();

  if (common.isWindows) {
    worker1.on('error', common.mustCall((err) => {
      console.log(err);
      assert.strictEqual(err.code, 'ENOTSUP');
      worker1.kill();
    }));
    return;
  }

  worker1.on('message', common.mustCall((msg) => {
    console.log(msg);
    assert.strictEqual(msg, 'success');

    const worker2 = cluster.fork({ WORKER2_NAME });
    worker2.on('message', common.mustCall((msg) => {
      console.log(msg);
      assert.strictEqual(msg, 'socket3:EADDRINUSE');

      // finish test
      worker1.send(BYE);
      worker2.send(BYE);
    }));
    worker2.on('exit', common.mustCall((code, signal) => {
      assert.strictEqual(signal, null);
      assert.strictEqual(code, 0);
    }));
  }));
  worker1.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(signal, null);
    assert.strictEqual(code, 0);
  }));
  // end master code
} else {
  // worker code
  process.on('message', common.mustCallAtLeast((msg) => {
    if (msg === BYE) process.exit(0);
  }), 1);

  const isSecondWorker = process.env.WORKER2_NAME === WORKER2_NAME;
  const socket1 = dgram.createSocket('udp4', common.mustNotCall());
  const socket2 = dgram.createSocket('udp4', common.mustNotCall());
  const socket3 = dgram.createSocket('udp4', common.mustNotCall());

  socket1.on('error', (err) => assert.fail(err));
  socket2.on('error', (err) => assert.fail(err));

  // First worker should bind, second should err
  const socket3OnBind =
    isSecondWorker ?
      common.mustNotCall() :
      common.mustCall(() => {
        const port3 = socket3.address().port;
        assert.strictEqual(typeof port3, 'number');
        process.send('success');
      });
  // an error is expected only in the second worker
  const socket3OnError =
    !isSecondWorker ?
      common.mustNotCall() :
      common.mustCall((err) => {
        process.send(`socket3:${err.code}`);
      });
  const address = common.localhostIPv4;
  const opt1 = { address, port: 0, exclusive: false };
  const opt2 = { address, port: common.PORT, exclusive: false };
  const opt3 = { address, port: common.PORT + 1, exclusive: true };
  socket1.bind(opt1, common.mustCall(() => {
    const port1 = socket1.address().port;
    assert.strictEqual(typeof port1, 'number');
    socket2.bind(opt2, common.mustCall(() => {
      const port2 = socket2.address().port;
      assert.strictEqual(typeof port2, 'number');
      socket3.on('error', socket3OnError);
      socket3.bind(opt3, socket3OnBind);
    }));
  }));
}
