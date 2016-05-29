'use strict';
var common = require('../common');
var assert = require('assert');

var net = require('net');

var tests_run = 0;

function pingPongTest(port, host) {
  var N = 1000;
  var count = 0;
  var sentPongs = 0;
  var sent_final_ping = false;

  var server = net.createServer({ allowHalfOpen: true }, function(socket) {
    console.log('connection: ' + socket.remoteAddress);
    assert.equal(server, socket.server);
    assert.equal(1, server.connections);

    socket.setNoDelay();
    socket.timeout = 0;

    socket.setEncoding('utf8');
    socket.on('data', function(data) {
      // Since we never queue data (we're always waiting for the PING
      // before sending a pong) the writeQueueSize should always be less
      // than one message.
      assert.ok(0 <= socket.bufferSize && socket.bufferSize <= 4);

      assert.equal(true, socket.writable);
      assert.equal(true, socket.readable);
      assert.equal(true, count <= N);
      assert.equal(data, 'PING');

      socket.write('PONG', function() {
        sentPongs++;
      });
    });

    socket.on('end', function() {
      assert.equal(true, socket.allowHalfOpen);
      assert.equal(true, socket.writable); // because allowHalfOpen
      assert.equal(false, socket.readable);
      socket.end();
    });

    socket.on('error', function(e) {
      throw e;
    });

    socket.on('close', function() {
      console.log('server socket.end');
      assert.equal(false, socket.writable);
      assert.equal(false, socket.readable);
      socket.server.close();
    });
  });


  server.listen(port, host, function() {
    if (this.address().port)
      port = this.address().port;
    console.log(`server listening on ${port} ${host}`);

    var client = net.createConnection(port, host);

    client.setEncoding('ascii');
    client.on('connect', function() {
      assert.equal(true, client.readable);
      assert.equal(true, client.writable);
      client.write('PING');
    });

    client.on('data', function(data) {
      assert.equal('PONG', data);
      count += 1;

      if (sent_final_ping) {
        assert.equal(false, client.writable);
        assert.equal(true, client.readable);
        return;
      } else {
        assert.equal(true, client.writable);
        assert.equal(true, client.readable);
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
      console.log('client.end');
      assert.equal(N + 1, count);
      assert.equal(N + 1, sentPongs);
      assert.equal(true, sent_final_ping);
      tests_run += 1;
    });

    client.on('error', function(e) {
      throw e;
    });
  });
}

/* All are run at once, so run on different ports */
common.refreshTmpDir();
pingPongTest(common.PIPE);
pingPongTest(0);
pingPongTest(0, 'localhost');
if (common.hasIPv6)
  pingPongTest(0, '::1');

process.on('exit', function() {
  if (common.hasIPv6)
    assert.equal(4, tests_run);
  else
    assert.equal(3, tests_run);
  console.log('done');
});
