'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

var buf = new Buffer(10 * 1024 * 1024);

buf.fill(0x62);

var errs = [];

var srv = net.createServer(function onConnection(conn) {
  conn.write(buf);
  conn.on('error', function(err) {
    errs.push(err);
    if (errs.length > 1 && errs[0] === errs[1])
      assert(false, 'We should not be emitting the same error twice');
  });
  conn.on('close', function() {
    srv.unref();
  });
}).listen(common.PORT, function() {
  var client = net.connect({ port: common.PORT });

  client.on('connect', function() {
    client.destroy();
  });
});

process.on('exit', function() {
  console.log(errs);
  assert.equal(errs.length, 1);
});
