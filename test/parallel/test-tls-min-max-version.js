'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');

// Check min/max protocol versions.

const {
  assert, connect, keys, tls
} = require(fixtures.path('tls-connect'));
const DEFAULT_MIN_VERSION = tls.DEFAULT_MIN_VERSION;

function test(cmin, cmax, cprot, smin, smax, sprot, expect) {
  connect({
    client: {
      checkServerIdentity: (servername, cert) => { },
      ca: `${keys.agent1.cert}\n${keys.agent6.ca}`,
      minVersion: cmin,
      maxVersion: cmax,
      secureProtocol: cprot,
    },
    server: {
      cert: keys.agent6.cert,
      key: keys.agent6.key,
      minVersion: smin,
      maxVersion: smax,
      secureProtocol: sprot,
    },
  }, common.mustCall((err, pair, cleanup) => {
    if (expect && expect.match(/^ERR/)) {
      assert.strictEqual(err.code, expect);
      return cleanup();
    }

    if (expect) {
      assert.ifError(pair.server.err);
      assert.ifError(pair.client.err);
      assert(pair.server.conn);
      assert(pair.client.conn);
      assert.strictEqual(pair.client.conn.getProtocol(), expect);
      assert.strictEqual(pair.server.conn.getProtocol(), expect);
      return cleanup();
    }

    assert(pair.server.err);
    assert(pair.client.err);
    return cleanup();
  }));
}

const U = undefined;

// Default protocol is TLSv1.2.
test(U, U, U, U, U, U, 'TLSv1.2');

// Insecure or invalid protocols cannot be enabled.
test(U, U, U, U, U, 'SSLv2_method', 'ERR_TLS_INVALID_PROTOCOL_METHOD');
test(U, U, U, U, U, 'SSLv3_method', 'ERR_TLS_INVALID_PROTOCOL_METHOD');
test(U, U, 'SSLv2_method', U, U, U, 'ERR_TLS_INVALID_PROTOCOL_METHOD');
test(U, U, 'SSLv3_method', U, U, U, 'ERR_TLS_INVALID_PROTOCOL_METHOD');
test(U, U, 'hokey-pokey', U, U, U, 'ERR_TLS_INVALID_PROTOCOL_METHOD');
test(U, U, U, U, U, 'hokey-pokey', 'ERR_TLS_INVALID_PROTOCOL_METHOD');

// Cannot use secureProtocol and min/max versions simultaneously.
test(U, U, U, U, 'TLSv1.2', 'TLS1_2_method',
     'ERR_TLS_PROTOCOL_VERSION_CONFLICT');
test(U, U, U, 'TLSv1.2', U, 'TLS1_2_method',
     'ERR_TLS_PROTOCOL_VERSION_CONFLICT');
test(U, 'TLSv1.2', 'TLS1_2_method', U, U, U,
     'ERR_TLS_PROTOCOL_VERSION_CONFLICT');
test('TLSv1.2', U, 'TLS1_2_method', U, U, U,
     'ERR_TLS_PROTOCOL_VERSION_CONFLICT');

// TLS_method means "any supported protocol".
test(U, U, 'TLSv1_2_method', U, U, 'TLS_method', 'TLSv1.2');
test(U, U, 'TLSv1_1_method', U, U, 'TLS_method', 'TLSv1.1');
test(U, U, 'TLSv1_method', U, U, 'TLS_method', 'TLSv1');
test(U, U, 'TLS_method', U, U, 'TLSv1_2_method', 'TLSv1.2');
test(U, U, 'TLS_method', U, U, 'TLSv1_1_method', 'TLSv1.1');
test(U, U, 'TLS_method', U, U, 'TLSv1_method', 'TLSv1');

// SSLv23 also means "any supported protocol" greater than the default
// minimum (which is configurable via command line).
test(U, U, 'TLSv1_2_method', U, U, 'SSLv23_method', 'TLSv1.2');

if (DEFAULT_MIN_VERSION === 'TLSv1.2') {
  test(U, U, 'TLSv1_1_method', U, U, 'SSLv23_method', null);
  test(U, U, 'TLSv1_method', U, U, 'SSLv23_method', null);
  test(U, U, 'SSLv23_method', U, U, 'TLSv1_1_method', null);
  test(U, U, 'SSLv23_method', U, U, 'TLSv1_method', null);
}

if (DEFAULT_MIN_VERSION === 'TLSv1.1') {
  test(U, U, 'TLSv1_1_method', U, U, 'SSLv23_method', 'TLSv1.1');
  test(U, U, 'TLSv1_method', U, U, 'SSLv23_method', null);
  test(U, U, 'SSLv23_method', U, U, 'TLSv1_1_method', 'TLSv1.1');
  test(U, U, 'SSLv23_method', U, U, 'TLSv1_method', null);
}

if (DEFAULT_MIN_VERSION === 'TLSv1') {
  test(U, U, 'TLSv1_1_method', U, U, 'SSLv23_method', 'TLSv1.1');
  test(U, U, 'TLSv1_method', U, U, 'SSLv23_method', 'TLSv1');
  test(U, U, 'SSLv23_method', U, U, 'TLSv1_1_method', 'TLSv1.1');
  test(U, U, 'SSLv23_method', U, U, 'TLSv1_method', 'TLSv1');
}

// TLSv1 thru TLSv1.2 are only supported with explicit configuration with API or
// CLI (--tls-v1.0 and --tls-v1.1).
test(U, U, 'TLSv1_2_method', U, U, 'TLSv1_2_method', 'TLSv1.2');
test(U, U, 'TLSv1_1_method', U, U, 'TLSv1_1_method', 'TLSv1.1');
test(U, U, 'TLSv1_method', U, U, 'TLSv1_method', 'TLSv1');

// The default default.
if (DEFAULT_MIN_VERSION === 'TLSv1.2') {
  test(U, U, 'TLSv1_1_method', U, U, U, null);
  test(U, U, 'TLSv1_method', U, U, U, null);
  test(U, U, U, U, U, 'TLSv1_1_method', null);
  test(U, U, U, U, U, 'TLSv1_method', null);
}

// The default with --tls-v1.1.
if (DEFAULT_MIN_VERSION === 'TLSv1.1') {
  test(U, U, 'TLSv1_1_method', U, U, U, 'TLSv1.1');
  test(U, U, 'TLSv1_method', U, U, U, null);
  test(U, U, U, U, U, 'TLSv1_1_method', 'TLSv1.1');
  test(U, U, U, U, U, 'TLSv1_method', null);
}

// The default with --tls-v1.0.
if (DEFAULT_MIN_VERSION === 'TLSv1') {
  test(U, U, 'TLSv1_1_method', U, U, U, 'TLSv1.1');
  test(U, U, 'TLSv1_method', U, U, U, 'TLSv1');
  test(U, U, U, U, U, 'TLSv1_1_method', 'TLSv1.1');
  test(U, U, U, U, U, 'TLSv1_method', 'TLSv1');
}

// TLS min/max are respected when set with no secureProtocol.
test('TLSv1', 'TLSv1.2', U, U, U, 'TLSv1_method', 'TLSv1');
test('TLSv1', 'TLSv1.2', U, U, U, 'TLSv1_1_method', 'TLSv1.1');
test('TLSv1', 'TLSv1.2', U, U, U, 'TLSv1_2_method', 'TLSv1.2');

test(U, U, 'TLSv1_method', 'TLSv1', 'TLSv1.2', U, 'TLSv1');
test(U, U, 'TLSv1_1_method', 'TLSv1', 'TLSv1.2', U, 'TLSv1.1');
test(U, U, 'TLSv1_2_method', 'TLSv1', 'TLSv1.2', U, 'TLSv1.2');

test('TLSv1', 'TLSv1.1', U, 'TLSv1', 'TLSv1.2', U, 'TLSv1.1');
test('TLSv1', 'TLSv1.2', U, 'TLSv1', 'TLSv1.1', U, 'TLSv1.1');
test('TLSv1', 'TLSv1', U, 'TLSv1', 'TLSv1.1', U, 'TLSv1');
test('TLSv1', 'TLSv1.2', U, 'TLSv1', 'TLSv1', U, 'TLSv1');
test('TLSv1.1', 'TLSv1.1', U, 'TLSv1', 'TLSv1.2', U, 'TLSv1.1');
test('TLSv1', 'TLSv1.2', U, 'TLSv1.1', 'TLSv1.1', U, 'TLSv1.1');
