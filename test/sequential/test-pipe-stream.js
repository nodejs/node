'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

common.refreshTmpDir();

function test(clazz, cb) {
  var have_ping = false;
  var have_pong = false;

  function check() {
    assert.ok(have_ping);
    assert.ok(have_pong);
  }

  function ping() {
    var conn = new clazz();

    conn.on('error', function(err) {
      throw err;
    });

    conn.connect(common.PIPE, function() {
      conn.write('PING', 'utf-8');
    });

    conn.on('data', function(data) {
      assert.equal(data.toString(), 'PONG');
      have_pong = true;
      conn.destroy();
    });
  }

  function pong(conn) {
    conn.on('error', function(err) {
      throw err;
    });

    conn.on('data', function(data) {
      assert.equal(data.toString(), 'PING');
      have_ping = true;
      conn.write('PONG', 'utf-8');
    });

    conn.on('close', function() {
      server.close();
    });
  }

  var timeout = setTimeout(function() {
    server.close();
  }, 2000);

  var server = net.Server();
  server.listen(common.PIPE, ping);
  server.on('connection', pong);
  server.on('close', function() {
    clearTimeout(timeout);
    check();
    cb && cb();
  });
}

test(net.Stream, function() {
  test(net.Socket);
});

