'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// Regression tests for https://github.com/nodejs/node/issues/40693

const assert = require('assert');
const fixtures = require('../common/fixtures');
const tls = require('tls');
const { AsyncLocalStorage } = require('async_hooks');

const options = {
  cert: fixtures.readKey('rsa_cert.crt'),
  key: fixtures.readKey('rsa_private.pem'),
  rejectUnauthorized: false,
};

tls
  .createServer(options, (socket) => {
    socket.write('Hello, world!');
    socket.pipe(socket);
  })
  .listen(0, function() {
    const asyncLocalStorage = new AsyncLocalStorage();
    const store = { val: 'abcd' };
    asyncLocalStorage.run(store, () => {
      const client = tls.connect({ port: this.address().port, ...options });
      client.on('data', () => {
        assert.deepStrictEqual(asyncLocalStorage.getStore(), store);
        client.end();
        this.close();
      });
    });
  });
