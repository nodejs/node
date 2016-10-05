'use strict';
const common = require('../common');
const assert = require('assert');

const net = require('net');

// This test creates 20 connections to a server and sets the server's
// maxConnections property to 10. The first 10 connections make it through
// and the last 10 connections are rejected.

const N = 20;
var closes = 0;
const waits = [];

const server = net.createServer(common.mustCall(function(connection) {
  connection.write('hello');
  waits.push(function() { connection.end(); });
}, N / 2));

server.listen(0, function() {
  makeConnection(0);
});

server.maxConnections = N / 2;


function makeConnection(index) {
  const c = net.createConnection(server.address().port);
  var gotData = false;

  c.on('connect', function() {
    if (index + 1 < N) {
      makeConnection(index + 1);
    }

    c.on('close', function() {
      console.error('closed %d', index);
      closes++;

      if (closes < N / 2) {
        assert.ok(
          server.maxConnections <= index,
          `${index} should not have been one of the first closed connections`
        );
      }

      if (closes === N / 2) {
        var cb;
        console.error('calling wait callback.');
        while (cb = waits.shift()) {
          cb();
        }
        server.close();
      }

      if (index < server.maxConnections) {
        assert.strictEqual(true, gotData,
                           `${index} didn't get data, but should have`);
      } else {
        assert.strictEqual(false, gotData,
                           `${index} got data, but shouldn't have`);
      }
    });
  });

  c.on('end', function() { c.end(); });

  c.on('data', function(b) {
    gotData = true;
    assert.ok(0 < b.length);
  });

  c.on('error', function(e) {
    // Retry if SmartOS and ECONNREFUSED. See
    // https://github.com/nodejs/node/issues/2663.
    if (common.isSunOS && (e.code === 'ECONNREFUSED')) {
      c.connect(server.address().port);
    }
    console.error('error %d: %s', index, e);
  });
}


process.on('exit', function() {
  assert.strictEqual(N, closes);
});
