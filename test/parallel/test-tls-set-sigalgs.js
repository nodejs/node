'use strict';
const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');
const fixtures = require('../common/fixtures');

// Test sigalgs: option for TLS.

const {
  assert, connect, keys
} = require(fixtures.path('tls-connect'));

function assert_arrays_equal(left, right) {
  assert.strictEqual(left.length, right.length);
  for (let i = 0; i < left.length; i++) {
    assert.strictEqual(left[i], right[i]);
  }
}

function test(csigalgs, ssigalgs, shared_sigalgs, cerr, serr) {
  assert(shared_sigalgs || serr || cerr, 'test missing any expectations');
  connect({
    client: {
      checkServerIdentity: (servername, cert) => { },
      ca: `${keys.agent1.cert}\n${keys.agent6.ca}`,
      cert: keys.agent2.cert,
      key: keys.agent2.key,
      sigalgs: csigalgs
    },
    server: {
      cert: keys.agent6.cert,
      key: keys.agent6.key,
      ca: keys.agent2.ca,
      context: {
        requestCert: true,
        rejectUnauthorized: true
      },
      sigalgs: ssigalgs
    },
  }, common.mustCall((err, pair, cleanup) => {
    if (shared_sigalgs) {
      assert.ifError(err);
      assert.ifError(pair.server.err);
      assert.ifError(pair.client.err);
      assert(pair.server.conn);
      assert(pair.client.conn);
      assert_arrays_equal(pair.server.conn.getSharedSigalgs(), shared_sigalgs);
    } else {
      if (serr) {
        assert(pair.server.err);
        assert(pair.server.err.code, serr);
      }

      if (cerr) {
        assert(pair.client.err);
        assert(pair.client.err.code, cerr);
      }
    }

    return cleanup();
  }));
}

// Have shared sigalgs
test('RSA-PSS+SHA384', 'RSA-PSS+SHA384', ['RSA-PSS+SHA384']);
test('RSA-PSS+SHA256:RSA-PSS+SHA512:ECDSA+SHA256',
     'RSA-PSS+SHA256:ECDSA+SHA256',
     ['RSA-PSS+SHA256', 'ECDSA+SHA256']);

// Do not have shared sigalgs.
test('RSA-PSS+SHA384', 'ECDSA+SHA256',
     undefined, 'ECONNRESET', 'ERR_SSL_NO_SHARED_SIGNATURE_ALGORITMS');

test('RSA-PSS+SHA384:ECDSA+SHA256', 'ECDSA+SHA384:RSA-PSS+SHA256',
     undefined, 'ECONNRESET', 'ERR_SSL_NO_SHARED_SIGNATURE_ALGORITMS');
