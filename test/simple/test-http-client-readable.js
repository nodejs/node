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

  this.createConnection = FakeAgent.prototype.createConnection;
}
util.inherits(FakeAgent, http.Agent);

FakeAgent.prototype.createConnection = function createConnection() {
  var s = new Duplex();

  function ondata(str)  {
    var buf = new Buffer(str);
    s.ondata(buf, 0, buf.length);
  }

  Object.defineProperty(s, 'ondata', {
    configurable: true,
    set: function(value) {
      Object.defineProperty(s, 'ondata', { value: value });

      process.nextTick(function() {
        ondata('HTTP/1.1 200 Ok\r\nTransfer-Encoding: chunked\r\n\r\n');

        s.readable = false;
        ondata('b\r\nhello world\r\n');
        ondata('b\r\n ohai world\r\n');
        ondata('0\r\n\r\n');
      });
    }
  });

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
var response;

var req = http.request({
  agent: new FakeAgent()
}, function(res) {
  response = res;

  res.on('readable', function() {
    var chunk = res.read();
    if (chunk !== null)
      received += chunk;
  });

  res.on('end', function() {
    ended++;
  });
});
req.end();

process.on('exit', function() {
  assert.equal(received, 'hello world ohai world');
  assert.equal(ended, 1);
});
