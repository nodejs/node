'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const https = require('https');

if (process.argv[2] === 'localhost') {
  const request = https.get('https://localhost:' + process.argv[3]);

  request.on('socket', (socket) => {
    socket.unref();
  });
} else {
  const assert = require('assert');
  const net = require('net');
  const server = net.createServer();
  server.listen(0);
  server.on('listening', () => {
    const port = server.address().port;
    const { fork } = require('child_process');
    const child = fork(__filename, ['localhost', port], {});
    child.on('close', (exit_code) => {
      server.close();
      assert.strictEqual(exit_code, 0);
    });
  });
}
