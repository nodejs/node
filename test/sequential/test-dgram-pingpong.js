'use strict';
const common = require('../common');
const assert = require('assert');
const Buffer = require('buffer').Buffer;
const dgram = require('dgram');

var tests_run = 0;

const intervals = new Map();

function clientSend(client, port) {
  const buf = new Buffer('PING');

  client.send(buf, 0, buf.length, port, 'localhost', function(err, bytes) {
    if (err) {
      // The setInterval might send a message after the server closes, so
      // ECANCELED is OK.
      if (err.code !== 'ECANCELED')
        throw err;
    }
  });
}

function pingPongTest(port, host) {
  var callbacks = 0;
  const N = 500;
  var count = 0;

  const server = dgram.createSocket('udp4', function(msg, rinfo) {
    if (/PING/.exec(msg)) {
      const buf = new Buffer(4);
      buf.write('PONG');
      server.send(buf, 0, buf.length,
                  rinfo.port, rinfo.address,
                  function(err, sent) {
                    callbacks++;
                  });
    }
  });

  server.on('error', function(e) {
    throw e;
  });

  server.on('listening', function() {
    console.log('server listening on ' + port);

    const client = dgram.createSocket('udp4');

    client.on('message', function(msg, rinfo) {
      assert.equal('PONG', msg.toString('ascii'));

      count += 1;

      if (count < N) {
        clientSend(client, port);
      } else {
        clearInterval(intervals.get(port));
        intervals.delete(port);
        client.close();
      }
    });

    client.on('close', function() {
      console.log('client has closed, closing server');
      assert.equal(N, count);
      tests_run += 1;
      server.close();
      assert(N - 1, callbacks);
    });

    client.on('error', function(e) {
      throw e;
    });

    console.log('Client sending to ' + port);
    const intervalFunc = () => { clientSend(client, port); };
    intervals.set(port, setInterval(intervalFunc, common.platformTimeout(1)));
    clientSend(client, port);
    count += 1;
  });
  server.bind(port, host);
}

// All are run at once, so run on different ports
pingPongTest(common.PORT + 0, 'localhost');
pingPongTest(common.PORT + 1, 'localhost');
pingPongTest(common.PORT + 2);
//pingPongTest('/tmp/pingpong.sock');

process.on('exit', function() {
  assert.equal(3, tests_run);
  console.log('done');
});
