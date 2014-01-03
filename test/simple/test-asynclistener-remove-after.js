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
var net = require('net');


// TODO(trevnorris): Test has the flaw that it's not checking if the async
// flag has been removed on the class instance. Though currently there's
// no way to do that.
var listener = process.addAsyncListener({ create: function() { }});


// Test timers

setImmediate(function() {
  assert.equal(this._asyncQueue.length, 0);
}).removeAsyncListener(listener);

setTimeout(function() {
  assert.equal(this._asyncQueue.length, 0);
}).removeAsyncListener(listener);

setInterval(function() {
  clearInterval(this);
  assert.equal(this._asyncQueue.length, 0);
}).removeAsyncListener(listener);


// Test net

var server = net.createServer(function(c) {
  c._handle.removeAsyncListener(listener);
  assert.equal(c._handle._asyncQueue.length, 0);
});

server.listen(common.PORT, function() {
  server._handle.removeAsyncListener(listener);
  assert.equal(server._handle._asyncQueue.length, 0);

  var client = net.connect(common.PORT, function() {
    client._handle.removeAsyncListener(listener);
    assert.equal(client._handle._asyncQueue.length, 0);
    client.end();
    server.close();
  });
});
