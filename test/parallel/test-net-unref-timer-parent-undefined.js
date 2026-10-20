'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');
const fixtures = require('../common/fixtures');

// A TLS socket whose `_parent` is left `undefined` during teardown must not
// crash when reads land on it (`onStreamRead` -> `_unrefTimer`) or when it is
// destroyed (`_destroy`). Both walk the `_parent` chain.
// Refs: https://github.com/nodejs/node/issues/64490

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
};

const server = tls.createServer(options, common.mustCall((conn) => {
  setTimeout(() => conn.write('x'), 50);
}));

server.listen(0, common.mustCall(() => {
  const client = tls.connect({
    port: server.address().port,
    rejectUnauthorized: false,
  }, common.mustCall(() => {
    client._parent = undefined;
  }));

  client.on('data', common.mustCall(() => {
    server.close();
    client.destroy();
  }));

  client.on('error', common.mustNotCall());
}));
