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

const server = http.createServer(common.mustCall((req, res) => {
  const body = 'hello world\n';

  res.writeHead(200, { 'Content-Length': body.length });
  res.write(body);
  res.end();
}, 3));

const agent = new http.Agent({ maxSockets: 1 });
const headers = { 'connection': 'keep-alive' };
let name;

server.listen(0, common.mustCall(function() {
  name = agent.getName({ port: this.address().port });
  http.get({
    path: '/', headers: headers, port: this.address().port, agent: agent
  }, common.mustCall((response) => {
    assert.strictEqual(agent.sockets[name].length, 1);
    assert.strictEqual(agent.requests[name].length, 2);
    response.resume();
  }));

  http.get({
    path: '/', headers: headers, port: this.address().port, agent: agent
  }, common.mustCall((response) => {
    assert.strictEqual(agent.sockets[name].length, 1);
    assert.strictEqual(agent.requests[name].length, 1);
    response.resume();
  }));

  http.get({
    path: '/', headers: headers, port: this.address().port, agent: agent
  }, common.mustCall((response) => {
    response.on('end', common.mustCall(() => {
      assert.strictEqual(agent.sockets[name].length, 1);
      assert(!agent.requests.hasOwnProperty(name));
      server.close();
    }));
    response.resume();
  }));
}));

process.on('exit', () => {
  assert(!agent.sockets.hasOwnProperty(name));
  assert(!agent.requests.hasOwnProperty(name));
});
