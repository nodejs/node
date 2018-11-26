'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { fork } = require('child_process');

// This test ensures that trying to load extra certs won't throw even when
// there is no crypto support, i.e., built with "./configure --without-ssl".
if (process.argv[2] === 'child') {
  // exit
} else {
  const NODE_EXTRA_CA_CERTS = fixtures.path('keys', 'ca1-cert.pem');

  fork(
    __filename,
    ['child'],
    { env: Object.assign({}, process.env, { NODE_EXTRA_CA_CERTS }) },
  ).on('exit', common.mustCall(function(status) {
    // client did not succeed in connecting
    assert.strictEqual(status, 0);
  }));
}
