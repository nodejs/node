var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;
var net = require('net');

function expect(activeHandles, activeRequests) {
  assert.equal(process._getActiveHandles().length, activeHandles);
  assert.equal(process._getActiveRequests().length, activeRequests);
}

var handles = [];

(function() {
  expect(0, 0);
  var server = net.createServer().listen(common.PORT);
  expect(1, 0);
  server.close();
  expect(1, 0); // server handle doesn't shut down until next tick
  handles.push(server);
})();

(function() {
  function onlookup() {
    setImmediate(function() {
      assert.equal(process._getActiveRequests().length, 0);
    });
  };

  expect(1, 0);
  var conn = net.createConnection(common.PORT);
  conn.on('lookup', onlookup);
  conn.on('error', function() { assert(false); });
  expect(2, 1);
  conn.destroy();
  expect(2, 1); // client handle doesn't shut down until next tick
  handles.push(conn);
})();

(function() {
  var n = 0;

  handles.forEach(function(handle) {
    handle.once('close', onclose);
  });
  function onclose() {
    if (++n === handles.length) {
      // Allow the server handle a few loop iterations to wind down.
      setImmediate(function() {
        setImmediate(function() {
          assert.equal(process._getActiveHandles().length, 0);
        });
      });
    }
  }
})();
