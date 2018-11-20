'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');

{
  const server = tls.createServer();
  assert.strictEqual(server.clientCertEngine, undefined);
  server.setOptions({ clientCertEngine: 'Cannonmouth' });
  assert.strictEqual(server.clientCertEngine, 'Cannonmouth');
}
