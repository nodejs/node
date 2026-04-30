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
const net = require('net');

const pingPongTest = common.mustCall((host, on_complete) => {
  const N = 1000;
  let count = 0;
  let sent_final_ping = false;

  const server = net.createServer({ allowHalfOpen: true }, common.mustCall((socket) => {
    assert.strictEqual(socket.remoteAddress !== null, true);
    assert.strictEqual(socket.remoteAddress !== undefined, true);
    const address = socket.remoteAddress;
    if (host === '127.0.0.1') {
      assert.strictEqual(address, '127.0.0.1');
    } else if (host == null || host === 'localhost') {
      assert(address === '127.0.0.1' || address === '::ffff:127.0.0.1' ||
        address === '::1');
    } else {
      console.log(`host = ${host}, remoteAddress = ${address}`);
      assert.strictEqual(address, '::1');
    }

    socket.setEncoding('utf8');
    socket.setNoDelay();
    socket.timeout = 0;

    socket.on('data', common.mustCallAtLeast((data) => {
      console.log(`server got: ${JSON.stringify(data)}`);
      assert.strictEqual(socket.readyState, 'open');
      assert.strictEqual(count <= N, true);
      if (/PING/.test(data)) {
        socket.write('PONG');
      }
    }, N + 1));

    socket.on('end', common.mustCall(() => {
      assert.strictEqual(socket.readyState, 'writeOnly');
      socket.end();
    }));

    socket.on('close', common.mustCall((had_error) => {
      assert.strictEqual(had_error, false);
      assert.strictEqual(socket.readyState, 'closed');
      socket.server.close();
    }));
  }));

  server.listen(0, host, common.mustCall(() => {
    const client = net.createConnection(server.address().port, host);

    client.setEncoding('utf8');

    client.on('connect', common.mustCall(() => {
      assert.strictEqual(client.readyState, 'open');
      client.write('PING');
    }));

    client.on('data', common.mustCallAtLeast((data) => {
      console.log(`client got: ${data}`);

      assert.strictEqual(data, 'PONG');
      count += 1;

      if (sent_final_ping) {
        assert.strictEqual(client.readyState, 'readOnly');
        return;
      }
      assert.strictEqual(client.readyState, 'open');

      if (count < N) {
        client.write('PING');
      } else {
        sent_final_ping = true;
        client.write('PING');
        client.end();
      }
    }));

    client.on('close', common.mustCall(() => {
      assert.strictEqual(count, N + 1);
      assert.strictEqual(sent_final_ping, true);
      if (on_complete) on_complete();
    }));
  }));
}, common.hasIPv6 ? 3 : 2);

// All are run at once and will run on different ports.
pingPongTest(null);
pingPongTest('127.0.0.1');
if (common.hasIPv6) pingPongTest('::1');
