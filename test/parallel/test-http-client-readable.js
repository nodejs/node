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

FakeAgent.prototype.createConnection = function createConnection() {
  const s = new Duplex();
  let once = false;

  s._read = function _read() {
    if (once)
      return this.push(null);
    once = true;

    this.push('HTTP/1.1 200 Ok\r\nTransfer-Encoding: chunked\r\n\r\n');
    this.push('b\r\nhello world\r\n');
    this.readable = false;
    this.push('0\r\n\r\n');
  };

  // Blackhole
  s._write = function _write(data, enc, cb) {
    cb();
  };

  s.destroy = s.destroySoon = function destroy() {
    this.writable = false;
  };

  return s;
};

let received = '';

const req = http.request({
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
