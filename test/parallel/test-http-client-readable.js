'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const util = require('util');

const Duplex = require('stream').Duplex;

function FakeAgent() {
  http.Agent.call(this);
}
util.inherits(FakeAgent, http.Agent);

FakeAgent.prototype.createConnection = function() {
  const s = new Duplex();
  var once = false;

  s._read = function() {
    if (once)
      return this.push(null);
    once = true;

    this.push('HTTP/1.1 200 Ok\r\nTransfer-Encoding: chunked\r\n\r\n');
    this.push('b\r\nhello world\r\n');
    this.readable = false;
    this.push('0\r\n\r\n');
  };

  // Blackhole
  s._write = function(data, enc, cb) {
    cb();
  };

  s.destroy = s.destroySoon = function() {
    this.writable = false;
  };

  return s;
};

var received = '';

const req = http.request({
  agent: new FakeAgent()
}, common.mustCall(function requestCallback(res) {
  res.on('data', function dataCallback(chunk) {
    received += chunk;
  });

  res.on('end', common.mustCall(function endCallback() {
    assert.strictEqual(received, 'hello world');
  }));
}));
req.end();
