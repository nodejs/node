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
var dns = require('dns');
var fs = require('fs');
var net = require('net');

var errorMsgs = [];
var caught = 0;
var expectCaught = 0;
var exitCbRan = false;

function asyncL() { }

var callbacksObj = {
  error: function(value, er) {
    var idx = errorMsgs.indexOf(er.message);
    caught++;

    process._rawDebug('Handling error: ' + er.message);

    if (-1 < idx)
      errorMsgs.splice(idx, 1);
    else
      throw new Error('Message not found: ' + er.message);

    return true;
  }
};

var listener = process.createAsyncListener(asyncL, callbacksObj);

process.on('exit', function(code) {
  // Just in case.
  process.removeAsyncListener(listener);

  if (code > 0)
    return;

  assert.ok(!exitCbRan);
  exitCbRan = true;

  if (errorMsgs.length > 0)
    throw new Error('Errors not fired: ' + errorMsgs);

  assert.equal(caught, expectCaught);
  process._rawDebug('ok');
});


// Simple cases
errorMsgs.push('setTimeout - simple');
errorMsgs.push('setImmediate - simple');
errorMsgs.push('setInterval - simple');
setTimeout(function() {
  throw new Error('setTimeout - simple');
}).addAsyncListener(listener);
expectCaught++;

setImmediate(function() {
  throw new Error('setImmediate - simple');
}).addAsyncListener(listener);
expectCaught++;

var b = setInterval(function() {
  clearInterval(b);
  throw new Error('setInterval - simple');
}).addAsyncListener(listener);
expectCaught++;


// Deeply nested
errorMsgs.push('setInterval - nested');
errorMsgs.push('setImmediate - nested');
errorMsgs.push('setTimeout - nested');
setTimeout(function() {
  setImmediate(function() {
    var b = setInterval(function() {
      clearInterval(b);
      throw new Error('setInterval - nested');
    }).addAsyncListener(listener);
    expectCaught++;
    throw new Error('setImmediate - nested');
  }).addAsyncListener(listener);
  expectCaught++;
  throw new Error('setTimeout - nested');
}).addAsyncListener(listener);
expectCaught++;


// Net
var iter = 3;
for (var i = 0; i < iter; i++) {
  errorMsgs.push('net - error: server connection');
  errorMsgs.push('net - error: client data');
  errorMsgs.push('net - error: server data');
}
errorMsgs.push('net - error: server closed');

var server = net.createServer(function(c) {
  c._handle.addAsyncListener(listener);

  c.on('data', function() {
    if (0 === --iter) {
      server.close(function() {
        process._rawDebug('net - server closing');
        throw new Error('net - error: server closed');
      });
      expectCaught++;
    }
    process._rawDebug('net - connection received data');
    throw new Error('net - error: server data');
  });
  expectCaught++;

  c.end('bye');
  process._rawDebug('net - connection received');
  throw new Error('net - error: server connection');
});
expectCaught += iter;

server.listen(common.PORT, function() {
  // Test adding the async listener after server creation. Though it
  // won't catch errors that originate synchronously from this point.
  server._handle.addAsyncListener(listener);
  for (var i = 0; i < iter; i++)
    clientConnect();
});

function clientConnect() {
  var client = net.connect(common.PORT, function() {
    client._handle.addAsyncListener(listener);
  });

  client.on('data', function() {
    client.end('see ya');
    process._rawDebug('net - client received data');
    throw new Error('net - error: client data');
  });
  expectCaught++;
}
