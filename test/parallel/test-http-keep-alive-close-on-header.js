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
require('../common');
const assert = require('assert');
const http = require('http');

const body = 'hello world\n';
const headers = { 'connection': 'keep-alive' };

const server = http.createServer(function(req, res) {
  res.writeHead(200, { 'Content-Length': body.length, 'Connection': 'close' });
  res.write(body);
  res.end();
});

let connectCount = 0;


server.listen(0, function() {
  const agent = new http.Agent({ maxSockets: 1 });
  const name = agent.getName({ port: this.address().port });
  let request = http.request({
    method: 'GET',
    path: '/',
    headers: headers,
    port: this.address().port,
    agent: agent
  }, function(res) {
    assert.strictEqual(1, agent.sockets[name].length);
    res.resume();
  });
  request.on('socket', function(s) {
    s.on('connect', function() {
      connectCount++;
    });
  });
  request.end();

  request = http.request({
    method: 'GET',
    path: '/',
    headers: headers,
    port: this.address().port,
    agent: agent
  }, function(res) {
    assert.strictEqual(1, agent.sockets[name].length);
    res.resume();
  });
  request.on('socket', function(s) {
    s.on('connect', function() {
      connectCount++;
    });
  });
  request.end();
  request = http.request({
    method: 'GET',
    path: '/',
    headers: headers,
    port: this.address().port,
    agent: agent
  }, function(response) {
    response.on('end', function() {
      assert.strictEqual(1, agent.sockets[name].length);
      server.close();
    });
    response.resume();
  });
  request.on('socket', function(s) {
    s.on('connect', function() {
      connectCount++;
    });
  });
  request.end();
});

process.on('exit', function() {
  assert.strictEqual(3, connectCount);
});
