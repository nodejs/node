'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

var tests_run = 0;

function pingPongTest(port, host, on_complete) {
  var N = 1000;
  var count = 0;
  var sent_final_ping = false;

  var server = net.createServer({ allowHalfOpen: true }, function(socket) {
    assert.equal(true, socket.remoteAddress !== null);
    assert.equal(true, socket.remoteAddress !== undefined);
    var address = socket.remoteAddress;
    if (host === '127.0.0.1') {
      assert.equal(address, '127.0.0.1');
    } else if (host == null || host === 'localhost') {
      assert(address === '127.0.0.1' || address === '::ffff:127.0.0.1');
    } else {
      console.log('host = ' + host + ', remoteAddress = ' + address);
      assert.equal(address, '::1');
    }

    socket.setEncoding('utf8');
    socket.setNoDelay();
    socket.timeout = 0;

    socket.on('data', function(data) {
      console.log('server got: ' + JSON.stringify(data));
      assert.equal('open', socket.readyState);
      assert.equal(true, count <= N);
      if (/PING/.exec(data)) {
        socket.write('PONG');
      }
    });

    socket.on('end', function() {
      assert.equal('writeOnly', socket.readyState);
      socket.end();
    });

    socket.on('close', function(had_error) {
      assert.equal(false, had_error);
      assert.equal('closed', socket.readyState);
      socket.server.close();
    });
  });

  server.listen(port, host, function() {
    var client = net.createConnection(port, host);

    client.setEncoding('utf8');

    client.on('connect', function() {
      assert.equal('open', client.readyState);
      client.write('PING');
    });

    client.on('data', function(data) {
      console.log('client got: ' + data);

      assert.equal('PONG', data);
      count += 1;

      if (sent_final_ping) {
        assert.equal('readOnly', client.readyState);
        return;
      } else {
        assert.equal('open', client.readyState);
      }

      if (count < N) {
        client.write('PING');
      } else {
        sent_final_ping = true;
        client.write('PING');
        client.end();
      }
    });

    client.on('close', function() {
      assert.equal(N + 1, count);
      assert.equal(true, sent_final_ping);
      if (on_complete) on_complete();
      tests_run += 1;
    });
  });
}

/* All are run at once, so run on different ports */
pingPongTest(common.PORT, 'localhost');
pingPongTest(common.PORT + 1, null);

// This IPv6 isn't working on Solaris
var solaris = /sunos/i.test(process.platform);
if (!solaris) pingPongTest(common.PORT + 2, '::1');

process.on('exit', function() {
  assert.equal(solaris ? 2 : 3, tests_run);
});
