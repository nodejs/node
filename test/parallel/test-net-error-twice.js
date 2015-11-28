'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const buf = new Buffer(10 * 1024 * 1024);

buf.fill(0x62);

var errs = [];

const srv = net.createServer(function onConnection(conn) {
  process.nextTick(() => { conn.write(buf); });
  conn.on('error', function(err) {
    errs.push(err);
    if (errs.length > 1)
      assert(errs[0] !== errs[1], 'Should not get the same error twice');
  });
  conn.on('close', function() {
    srv.unref();
  });
}).listen(common.PORT, function() {
  const client = net.connect({ port: common.PORT });
  client.on('connect', client.destroy);
});

process.on('exit', function() {
  assert.equal(errs.length, 1);
});
