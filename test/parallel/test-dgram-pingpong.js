'use strict';
var common = require('../common');
var assert = require('assert');
var Buffer = require('buffer').Buffer;
var dgram = require('dgram');

var debug = false;
var tests_run = 0;

function pingPongTest(port, host) {
  var callbacks = 0;
  var N = 500;
  var count = 0;
  var sent_final_ping = false;

  var server = dgram.createSocket('udp4', function(msg, rinfo) {
    if (debug) console.log('server got: ' + msg +
                           ' from ' + rinfo.address + ':' + rinfo.port);

    if (/PING/.exec(msg)) {
      var buf = new Buffer(4);
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
    console.log('server listening on ' + port + ' ' + host);

    var buf = new Buffer('PING'),
        client = dgram.createSocket('udp4');

    client.on('message', function(msg, rinfo) {
      if (debug) console.log('client got: ' + msg +
                             ' from ' + rinfo.address + ':' + rinfo.port);
      assert.equal('PONG', msg.toString('ascii'));

      count += 1;

      if (count < N) {
        client.send(buf, 0, buf.length, port, 'localhost');
      } else {
        sent_final_ping = true;
        client.send(buf, 0, buf.length, port, 'localhost', function() {
          client.close();
        });
      }
    });

    client.on('close', function() {
      console.log('client has closed, closing server');
      assert.equal(N, count);
      tests_run += 1;
      server.close();
      assert.equal(N - 1, callbacks);
    });

    client.on('error', function(e) {
      throw e;
    });

    console.log('Client sending to ' + port + ', localhost ' + buf);
    client.send(buf, 0, buf.length, port, 'localhost', function(err, bytes) {
      if (err) {
        throw err;
      }
      console.log('Client sent ' + bytes + ' bytes');
    });
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
