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
const http = require('http');
const net = require('net');
const assert = require('assert');

const server = http.createServer(common.mustCall(function(req, res) {
  res.end();
}, 4)).listen(0, '127.0.0.1', function() {
  let fn = common.mustCall(createConnection);
  http.get({ createConnection: fn }, function(res) {
    res.resume();
    fn = common.mustCall(createConnectionAsync);
    http.get({ createConnection: fn }, function(res) {
      res.resume();
      fn = common.mustCall(createConnectionBoth1);
      http.get({ createConnection: fn }, function(res) {
        res.resume();
        fn = common.mustCall(createConnectionBoth2);
        http.get({ createConnection: fn }, function(res) {
          res.resume();
          fn = common.mustCall(createConnectionError);
          http.get({ createConnection: fn }, function(res) {
            common.fail('Unexpected response callback');
          }).on('error', common.mustCall(function(err) {
            assert.strictEqual(err.message, 'Could not create socket');
            server.close();
          }));
        });
      });
    });
  });
});

function createConnection() {
  return net.createConnection(server.address().port, '127.0.0.1');
}

function createConnectionAsync(options, cb) {
  setImmediate(function() {
    cb(null, net.createConnection(server.address().port, '127.0.0.1'));
  });
}

function createConnectionBoth1(options, cb) {
  const socket = net.createConnection(server.address().port, '127.0.0.1');
  setImmediate(function() {
    cb(null, socket);
  });
  return socket;
}

function createConnectionBoth2(options, cb) {
  const socket = net.createConnection(server.address().port, '127.0.0.1');
  cb(null, socket);
  return socket;
}

function createConnectionError(options, cb) {
  process.nextTick(cb, new Error('Could not create socket'));
}
