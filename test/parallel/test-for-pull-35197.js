// todo

'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const http2 = require('http2');
if (!common.hasCrypto)
  common.skip('missing crypto');

const serverOptions = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};
const server = http2.createSecureServer(
    serverOptions,
    common.mustCall((req, res) => {
const request=req
      assert(true===request.socket.encrypted)
      assert("encrypted" in request.socket)

    }));
