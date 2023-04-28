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

let name;
const max = 3;
const agent = new http.Agent();

const server = http.Server(common.mustCall((req, res) => {
  if (req.url === '/0') {
    setTimeout(common.mustCall(() => {
      res.writeHead(200);
      res.end('Hello, World!');
    }), 100);
  } else {
    res.writeHead(200);
    res.end('Hello, World!');
  }
}, max));
server.listen(0, common.mustCall(() => {
  name = agent.getName({ port: server.address().port });
  for (let i = 0; i < max; ++i)
    request(i);
}));

const countdown = new Countdown(max, () => {
  assert(!(name in agent.sockets));
  assert(!(name in agent.requests));
  server.close();
});

function request(i) {
  const req = http.get({
    port: server.address().port,
    path: `/${i}`,
    agent
  }, function(res) {
    const socket = req.socket;
    socket.on('close', common.mustCall(() => {
      countdown.dec();
      if (countdown.remaining > 0) {
        assert.strictEqual(agent.sockets[name].includes(socket),
                           false);
      }
    }));
    res.resume();
  });
}
