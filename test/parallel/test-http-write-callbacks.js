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

let serverEndCb = false;
let serverIncoming = '';
const serverIncomingExpect = 'bazquuxblerg';

let clientEndCb = false;
let clientIncoming = '';
const clientIncomingExpect = 'asdffoobar';

process.on('exit', function() {
  assert(serverEndCb);
  assert.strictEqual(serverIncoming, serverIncomingExpect);
  assert(clientEndCb);
  assert.strictEqual(clientIncoming, clientIncomingExpect);
  console.log('ok');
});

// Verify that we get a callback when we do res.write(..., cb)
const server = http.createServer(function(req, res) {
  res.statusCode = 400;
  res.end('Bad Request.\nMust send Expect:100-continue\n');
});

server.on('checkContinue', function(req, res) {
  server.close();
  assert.strictEqual(req.method, 'PUT');
  res.writeContinue(function() {
    // continue has been written
    req.on('end', function() {
      res.write('asdf', function(er) {
        assert.ifError(er);
        res.write('foo', 'ascii', function(er) {
          assert.ifError(er);
          res.end(Buffer.from('bar'), 'buffer', function(er) {
            serverEndCb = true;
          });
        });
      });
    });
  });

  req.setEncoding('ascii');
  req.on('data', function(c) {
    serverIncoming += c;
  });
});

server.listen(0, function() {
  const req = http.request({
    port: this.address().port,
    method: 'PUT',
    headers: { 'expect': '100-continue' }
  });
  req.on('continue', function() {
    // ok, good to go.
    req.write('YmF6', 'base64', function(er) {
      assert.ifError(er);
      req.write(Buffer.from('quux'), function(er) {
        assert.ifError(er);
        req.end('626c657267', 'hex', function(er) {
          assert.ifError(er);
          clientEndCb = true;
        });
      });
    });
  });
  req.on('response', function(res) {
    // this should not come until after the end is flushed out
    assert(clientEndCb);
    res.setEncoding('ascii');
    res.on('data', function(c) {
      clientIncoming += c;
    });
  });
});
