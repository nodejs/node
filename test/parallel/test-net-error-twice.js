'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const buf = new Buffer(10 * 1024 * 1024);

buf.fill(0x62);

var errs = [];

const srv = net.createServer(function onConnection(conn) {
  if (common.isWindows) {
    // Windows-specific handling. See:
    //   * https://github.com/nodejs/node/pull/4062
    //   * https://github.com/nodejs/node/issues/4057
    setTimeout(() => { conn.write(buf); }, 100);
  } else {
    conn.write(buf);
  }

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
