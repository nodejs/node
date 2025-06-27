// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const dgram = require('dgram');
const { internalBinding } = require('internal/test/binding');
const { UV_UNKNOWN } = internalBinding('uv');

if (cluster.isPrimary) {
  cluster.fork();
} else {
  // When the socket attempts to bind, it requests a handle from the cluster.
  // Force the cluster to send back an error code.
  cluster._getServer = function(self, options, callback) {
    callback(UV_UNKNOWN);
  };

  const socket = dgram.createSocket('udp4');

  socket.on('error', common.mustCall((err) => {
    assert.match(err.toString(), /^Error: bind UNKNOWN 0\.0\.0\.0$/);
    process.nextTick(common.mustCall(() => {
      assert.strictEqual(socket._bindState, 0); // BIND_STATE_UNBOUND
      socket.close();
      cluster.worker.disconnect();
    }));
  }));

  socket.bind(common.mustNotCall('Socket should not bind.'));
}
