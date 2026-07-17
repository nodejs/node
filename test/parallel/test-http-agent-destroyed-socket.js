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
const http = require('http');
const Countdown = require('../common/countdown');

const server = http.createServer(common.mustCall((req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('Hello World\n');
}, 2)).listen(0, common.mustCall(() => {
  const agent = new http.Agent({ maxSockets: 1 });

  agent.on('free', common.mustCall(3));

  const requestOptions = {
    agent: agent,
    host: 'localhost',
    port: server.address().port,
    path: '/'
  };

  const request1 = http.get(requestOptions, common.mustCall((response) => {
    // Assert request2 is queued in the agent
    const key = agent.getName(requestOptions);
    assert.strictEqual(agent.requests[key].length, 1);
    response.resume();
    response.on('end', common.mustCall(() => {
      request1.socket.destroy();

      request1.socket.once('close', common.mustCall(() => {
        // Assert request2 was removed from the queue
        assert(!agent.requests[key]);
        process.nextTick(() => {
          // Assert that the same socket was not assigned to request2,
          // since it was destroyed.
          assert.notStrictEqual(request1.socket, request2.socket);
          assert(!request2.socket.destroyed, 'the socket is destroyed');
        });
      }));
    }));
  }));

  const request2 = http.get(requestOptions, common.mustCall((response) => {
    assert(!request2.socket.destroyed);
    assert(request1.socket.destroyed);
    // Assert not reusing the same socket, since it was destroyed.
    assert.notStrictEqual(request1.socket, request2.socket);
    const countdown = new Countdown(2, () => server.close());
    request2.socket.on('close', common.mustCall(() => countdown.dec()));
    response.on('end', common.mustCall(() => countdown.dec()));
    response.resume();
  }));
}));
