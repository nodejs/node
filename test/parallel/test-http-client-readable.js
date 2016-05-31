'use strict';
require('../common');
var assert = require('assert');
var http = require('http');

var Duplex = require('stream').Duplex;

class FakeAgent extends http.Agent {
  createConnection() {
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
  }
}

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
