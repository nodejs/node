'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const assert = require('assert');
const tls = require('tls');

function loadPEM(n) {
  return fixtures.readKey(`${n}.pem`);
}

const serverOptions = {
  key: loadPEM('agent2-key'),
  cert: loadPEM('agent2-cert'),
  ca: [ loadPEM('ca2-cert') ],
  requestCert: true,
  rejectUnauthorized: false,
};

let connections = 0;

const server = tls.createServer(serverOptions, (c) => {
  if (++connections === 3) {
    server.close();
  }
  if (c.servername === 'unknowncontext') {
    assert.strictEqual(c.authorized, false);
    return;
  }
  assert.strictEqual(c.authorized, true);
});

const secureContext = {
  key: loadPEM('agent1-key'),
  cert: loadPEM('agent1-cert'),
  ca: [ loadPEM('ca1-cert') ],
};
server.addContext('context1', secureContext);
server.addContext('context2', tls.createSecureContext(secureContext));

const clientOptionsBase = {
  key: loadPEM('agent1-key'),
  cert: loadPEM('agent1-cert'),
  ca: [ loadPEM('ca1-cert') ],
  rejectUnauthorized: false,
};

server.listen(0, common.mustCall(() => {
  const client1 = tls.connect({
    ...clientOptionsBase,
    port: server.address().port,
    servername: 'context1',
  }, common.mustCall(() => {
    client1.end();
  }));

  const client2 = tls.connect({
    ...clientOptionsBase,
    port: server.address().port,
    servername: 'context2',
  }, common.mustCall(() => {
    client2.end();
  }));

  const client3 = tls.connect({
    ...clientOptionsBase,
    port: server.address().port,
    servername: 'unknowncontext',
  }, common.mustCall(() => {
    client3.end();
  }));
}));
