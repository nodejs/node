'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');
const tls = require('tls');

const dftProtocol = {};

// test for immutable `opts`
{
  const opts = { foo: 'bar', NPNProtocols: [ 'http/1.1' ] };
  const server = https.createServer(opts);

  tls.convertNPNProtocols([ 'http/1.1' ], dftProtocol);
  assert.deepStrictEqual(opts, { foo: 'bar', NPNProtocols: [ 'http/1.1' ] });
  assert.strictEqual(server.NPNProtocols.compare(dftProtocol.NPNProtocols), 0);
}


// validate that `createServer` can work with the only argument requestListener
{
  const mustNotCall = common.mustNotCall();
  const server = https.createServer(mustNotCall);

  tls.convertNPNProtocols([ 'http/1.1', 'http/1.0' ], dftProtocol);
  assert.strictEqual(server.NPNProtocols.compare(dftProtocol.NPNProtocols), 0);
  assert.strictEqual(server.listeners('request').length, 1);
  assert.strictEqual(server.listeners('request')[0], mustNotCall);
}


// validate that `createServer` can work with no arguments
{
  const server = https.createServer();

  assert.strictEqual(server.NPNProtocols.compare(dftProtocol.NPNProtocols), 0);
  assert.strictEqual(server.listeners('request').length, 0);
}
