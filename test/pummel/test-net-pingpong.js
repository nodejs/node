'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

let tests_run = 0;

function pingPongTest(port, host, on_complete) {
  const N = 1000;
  let count = 0;
  let sent_final_ping = false;

  const server = net.createServer({ allowHalfOpen: true }, function(socket) {
    assert.strictEqual(true, socket.remoteAddress !== null);
    assert.strictEqual(true, socket.remoteAddress !== undefined);
    const address = socket.remoteAddress;
    if (host === '127.0.0.1') {
      assert.strictEqual(address, '127.0.0.1');
    } else if (host == null || host === 'localhost') {
      assert(address === '127.0.0.1' || address === '::ffff:127.0.0.1');
    } else {
      console.log('host = ' + host + ', remoteAddress = ' + address);
      assert.strictEqual(address, '::1');
    }

    socket.setEncoding('utf8');
    socket.setNoDelay();
    socket.timeout = 0;

    socket.on('data', function(data) {
      console.log('server got: ' + JSON.stringify(data));
      assert.strictEqual('open', socket.readyState);
      assert.strictEqual(true, count <= N);
      if (/PING/.exec(data)) {
        socket.write('PONG');
      }
    });

    socket.on('end', function() {
      assert.strictEqual('writeOnly', socket.readyState);
      socket.end();
    });

    socket.on('close', function(had_error) {
      assert.strictEqual(false, had_error);
      assert.strictEqual('closed', socket.readyState);
      socket.server.close();
    });
  });

  server.listen(port, host, function() {
    const client = net.createConnection(port, host);

    client.setEncoding('utf8');

    client.on('connect', function() {
      assert.strictEqual('open', client.readyState);
      client.write('PING');
    });

    client.on('data', function(data) {
      console.log('client got: ' + data);

      assert.strictEqual('PONG', data);
      count += 1;

      if (sent_final_ping) {
        assert.strictEqual('readOnly', client.readyState);
        return;
      } else {
        assert.strictEqual('open', client.readyState);
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
      assert.strictEqual(N + 1, count);
      assert.strictEqual(true, sent_final_ping);
      if (on_complete) on_complete();
      tests_run += 1;
    });
  });
}

/* All are run at once, so run on different ports */
pingPongTest(common.PORT, 'localhost');
pingPongTest(common.PORT + 1, null);

// This IPv6 isn't working on Solaris
if (!common.isSunOS) pingPongTest(common.PORT + 2, '::1');

process.on('exit', function() {
  assert.strictEqual(common.isSunOS ? 2 : 3, tests_run);
});
