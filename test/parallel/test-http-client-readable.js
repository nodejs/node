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
var util = require('util');

var Duplex = require('stream').Duplex;

function FakeAgent() {
  http.Agent.call(this);
}
util.inherits(FakeAgent, http.Agent);

FakeAgent.prototype.createConnection = function createConnection() {
  var s = new Duplex();
  var once = false;

  s._read = function read() {
    if (once)
      return this.push(null);
    once = true;

    this.push('HTTP/1.1 200 Ok\r\nTransfer-Encoding: chunked\r\n\r\n');
    this.push('b\r\nhello world\r\n');
    this.readable = false;
    this.push('0\r\n\r\n');
  };

  // Blackhole
  s._write = function write(data, enc, cb) {
    cb();
  };

  s.destroy = s.destroySoon = function destroy() {
    this.writable = false;
  };

  return s;
};

var received = '';
var ended = 0;

var req = http.request({
  agent: new FakeAgent()
}, function(res) {
  res.on('data', function(chunk) {
    received += chunk;
  });

  res.on('end', function() {
    ended++;
  });
});
req.end();

process.on('exit', function() {
  assert.equal(received, 'hello world');
  assert.equal(ended, 1);
});
