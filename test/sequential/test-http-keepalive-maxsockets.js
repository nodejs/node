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


const serverSockets = [];
const server = http.createServer(function(req, res) {
  if (!serverSockets.includes(req.socket)) {
    serverSockets.push(req.socket);
  }
  res.end(req.url);
});
server.listen(0, function() {
  const agent = http.Agent({
    keepAlive: true,
    maxSockets: 5,
    maxFreeSockets: 2
  });

  let closed = false;
  makeReqs(10, function(er) {
    assert.ifError(er);
    assert.strictEqual(count(agent.freeSockets), 2);
    assert.strictEqual(count(agent.sockets), 0);
    assert.strictEqual(serverSockets.length, 5);

    // Now make 10 more reqs.
    // should use the 2 free reqs from the pool first.
    makeReqs(10, function(er) {
      assert.ifError(er);
      assert.strictEqual(count(agent.freeSockets), 2);
      assert.strictEqual(count(agent.sockets), 0);
      assert.strictEqual(serverSockets.length, 8);

      agent.destroy();
      server.close(function() {
        closed = true;
      });
    });
  });

  process.on('exit', function() {
    assert(closed);
    console.log('ok');
  });

  // make 10 requests in parallel,
  // then 10 more when they all finish.
  function makeReqs(n, cb) {
    for (let i = 0; i < n; i++)
      makeReq(i, then);

    function then(er) {
      if (er)
        return cb(er);
      else if (--n === 0)
        setTimeout(cb, 100);
    }
  }

  function makeReq(i, cb) {
    http.request({
      port: server.address().port,
      path: `/${i}`,
      agent: agent
    }, function(res) {
      let data = '';
      res.setEncoding('ascii');
      res.on('data', function(c) {
        data += c;
      });
      res.on('end', function() {
        assert.strictEqual(data, `/${i}`);
        cb();
      });
    }).end();
  }

  function count(sockets) {
    return Object.keys(sockets).reduce(function(n, name) {
      return n + sockets[name].length;
    }, 0);
  }
});
