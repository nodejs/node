'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const net = require('node:net');
const tls = require('node:tls');

for (const rejectUnauthorized of [false, undefined]) {
  const server = net.createServer((socket) => {
    socket.resume();
  });

  server.listen(0, '127.0.0.1', common.mustCall(() => {
    const options = {
      host: '127.0.0.1',
      port: server.address().port,
    };
    if (rejectUnauthorized === false)
      options.rejectUnauthorized = false;
    const socket = tls.connect(options);

    socket.on('connect', common.mustCall(() => {
      // A premature secure event must report a missing certificate even when
      // neither a cipher nor a session has been established yet.
      socket.emit('secure');
      assert.strictEqual(socket.authorized, false);
      assert.strictEqual(socket.authorizationError,
                         'UNABLE_TO_GET_ISSUER_CERT');
    }));

    if (rejectUnauthorized === false) {
      socket.on('error', common.mustNotCall());
      socket.on('secureConnect', common.mustCall(() => socket.destroy()));
    } else {
      socket.on('secureConnect', common.mustNotCall());
      socket.on('error', common.mustCall((err) => {
        assert.strictEqual(err.code, 'UNABLE_TO_GET_ISSUER_CERT');
      }));
    }

    socket.on('close', common.mustCall(() => {
      server.close(common.mustCall());
    }));
  }));
}
