'use strict';

// This tests the errors thrown from TLSSocket.prototype.setServername

const common = require('../common');
const fixtures = require('../common/fixtures');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { connect } = require('tls');
const makeDuplexPair = require('../common/duplexpair');
const { clientSide } = makeDuplexPair();

const ca = fixtures.readKey('ca1-cert.pem');

const client = connect({
  socket: clientSide,
  ca,
  host: 'agent1'  // Hostname from certificate
});

[undefined, null, 1, true, {}].forEach((value) => {
  common.expectsError(() => {
    client.setServername(value);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "name" argument must be of type string'
  });
});
