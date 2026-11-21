'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

// Regression: setKeepAlive() silently ignored errors returned by the
// underlying handle (https://github.com/nodejs/node/issues/65529).
// It should throw like setTypeOfService() does, instead of reporting
// success when the operation failed.
if (common.isWindows) {
  common.skip('keep-alive errors are treated as best-effort on Windows');
} else {
  const server = net.createServer(common.mustCall((socket) => {
    socket.end();
  }));

  server.listen(0, common.mustCall(() => {
    const client = net.connect(server.address().port, common.mustCall(() => {
      // Make the handle report an error (EINVAL) for the keep-alive call.
      const originalSetKeepAlive = client._handle.setKeepAlive;
      client._handle.setKeepAlive = () => -22; // UV_EINVAL
      try {
        assert.throws(
          () => client.setKeepAlive(true, 1000),
          (err) => {
            assert.strictEqual(err.code, 'EINVAL');
            assert.strictEqual(err.syscall, 'setKeepAlive');
            return true;
          },
        );
      } finally {
        client._handle.setKeepAlive = originalSetKeepAlive;
      }
      client.end();
      server.close();
    }));
  }));
}
