'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

function pingPongTest(port, host) {

  const server = dgram.createSocket('udp4', common.mustCall((msg, rinfo) => {
    assert.strictEqual('PING', msg.toString('ascii'));
    server.send('PONG', 0, 4, rinfo.port, rinfo.address);
  }));

  server.on('error', function(e) {
    throw e;
  });

  server.on('listening', function() {
    console.log('server listening on ' + port);

    const client = dgram.createSocket('udp4');

    client.on('message', function(msg) {
      assert.strictEqual('PONG', msg.toString('ascii'));

      client.close();
      server.close();
    });

    client.on('error', function(e) {
      throw e;
    });

    console.log('Client sending to ' + port);

    function clientSend() {
      client.send('PING', 0, 4, port, 'localhost');
    }

    clientSend();
  });
  server.bind(port, host);
  return server;
}

const server = pingPongTest(common.PORT, 'localhost');
server.on('close', common.mustCall(pingPongTest.bind(undefined, common.PORT)));
