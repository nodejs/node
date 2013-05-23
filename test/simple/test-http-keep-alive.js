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

var common = require('../common');
var assert = require('assert');
var http = require('http');

var body = 'hello world\n';

var server = http.createServer(function(req, res) {
  res.writeHead(200, {'Content-Length': body.length});
  res.write(body);
  res.end();
});

var connectCount = 0;
var agent = new http.Agent({maxSockets: 1});
var headers = {'connection': 'keep-alive'};
var name = agent.getName({ port: common.PORT });

server.listen(common.PORT, function() {
  http.get({
    path: '/', headers: headers, port: common.PORT, agent: agent
  }, function(response) {
    assert.equal(agent.sockets[name].length, 1);
    assert.equal(agent.requests[name].length, 2);
    response.resume();
  });

  http.get({
    path: '/', headers: headers, port: common.PORT, agent: agent
  }, function(response) {
    assert.equal(agent.sockets[name].length, 1);
    assert.equal(agent.requests[name].length, 1);
    response.resume();
  });

  http.get({
    path: '/', headers: headers, port: common.PORT, agent: agent
  }, function(response) {
    response.on('end', function() {
      assert.equal(agent.sockets[name].length, 1);
      assert(!agent.requests.hasOwnProperty(name));
      server.close();
    });
    response.resume();
  });
});

process.on('exit', function() {
  assert(!agent.sockets.hasOwnProperty(name));
  assert(!agent.requests.hasOwnProperty(name));
});
