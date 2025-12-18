'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

function test(clazz, cb) {
  let have_ping = false;
  let have_pong = false;

  function check() {
    assert.ok(have_ping);
    assert.ok(have_pong);
  }

  function ping() {
    const conn = new clazz();

    conn.on('error', function(err) {
      throw err;
    });

    conn.connect(common.PIPE, function() {
      conn.write('PING', 'utf-8');
    });

    conn.on('data', function(data) {
      assert.strictEqual(data.toString(), 'PONG');
      have_pong = true;
      conn.destroy();
    });
  }

  function pong(conn) {
    conn.on('error', function(err) {
      throw err;
    });

    conn.on('data', function(data) {
      assert.strictEqual(data.toString(), 'PING');
      have_ping = true;
      conn.write('PONG', 'utf-8');
    });

    conn.on('close', function() {
      server.close();
    });
  }

  const server = net.Server();
  server.listen(common.PIPE, ping);
  server.on('connection', pong);
  server.on('close', function() {
    check();
    cb && cb();
  });
}

test(net.Stream, function() {
  test(net.Socket);
});
