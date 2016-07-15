'use strict';
const common = require('../common');
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

var req = http.request({
  agent: new FakeAgent()
}, common.mustCall(function(res) {
  res.on('data', function(chunk) {
    received += chunk;
  });

  res.on('end', common.mustCall(function() {}));
}));
req.end();

process.on('exit', function() {
  assert.equal(received, 'hello world');
});
