'use strict';
const { mustCall } = require('../common');
const net = require('net');
const child_process = require('child_process');
const assert = require('assert');
// preset server config
const netServerConfig = {
  allowHalfOpen: true,
  pauseOnConnect: false,
  noDelay: false,
  keepAliveInitialDelay: 1000,
  keepAlive: true,
};

if (process.argv[2] !== 'child') {
  const childProcess = child_process.fork(__filename, ['child']);
  const server = net.createServer(netServerConfig);
  server.listen(9999,
                mustCall(() => {
                  childProcess.send(null, server);
                }),
  );
} else {
  process.on(
    'message',
    mustCall((msg, handle) => {
      // Pass through all of net.Server config from parent process
      assert.ok(handle.allowHalfOpen, netServerConfig.allowHalfOpen);
      assert.ok(handle.pauseOnConnect, netServerConfig.pauseOnConnect);
      assert.ok(handle.noDelay, netServerConfig.noDelay);
      assert.ok(
        handle.keepAliveInitialDelay,
        netServerConfig.keepAliveInitialDelay,
      );
      assert.ok(handle.keepAlive, netServerConfig.keepAlive);
    }),
  );
}
