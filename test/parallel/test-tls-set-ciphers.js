'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const { inspect } = require('util');

// Test cipher: option for TLS.

const {
  assert, connect, keys
} = require(fixtures.path('tls-connect'));


function test(cciphers, sciphers, cipher, cerr, serr) {
  assert(cipher || cerr || serr, 'test missing any expectations');
  const where = inspect(new Error()).split('\n')[2].replace(/[^(]*/, '');
  connect({
    client: {
      checkServerIdentity: (servername, cert) => { },
      ca: `${keys.agent1.cert}\n${keys.agent6.ca}`,
      ciphers: cciphers,
    },
    server: {
      cert: keys.agent6.cert,
      key: keys.agent6.key,
      ciphers: sciphers,
    },
  }, common.mustCall((err, pair, cleanup) => {
    function u(_) { return _ === undefined ? 'U' : _; }
    console.log('test:', u(cciphers), u(sciphers),
                'expect', u(cipher), u(cerr), u(serr));
    console.log('  ', where);
    if (!cipher) {
      console.log('client', pair.client.err ? pair.client.err.code : undefined);
      console.log('server', pair.server.err ? pair.server.err.code : undefined);
      if (cerr) {
        assert(pair.client.err);
        assert.strictEqual(pair.client.err.code, cerr);
      }
      if (serr) {
        assert(pair.server.err);
        assert.strictEqual(pair.server.err.code, serr);
      }
      return cleanup();
    }

    const reply = 'So long and thanks for all the fish.';

    assert.ifError(err);
    assert.ifError(pair.server.err);
    assert.ifError(pair.client.err);
    assert(pair.server.conn);
    assert(pair.client.conn);
    assert.strictEqual(pair.client.conn.getCipher().name, cipher);
    assert.strictEqual(pair.server.conn.getCipher().name, cipher);

    pair.server.conn.write(reply);

    pair.client.conn.on('data', common.mustCall((data) => {
      assert.strictEqual(data.toString(), reply);
      return cleanup();
    }));
  }));
}

const U = undefined;

// Have shared ciphers.
test(U, 'AES256-SHA', 'AES256-SHA');
test('AES256-SHA', U, 'AES256-SHA');

test(U, 'TLS_AES_256_GCM_SHA384', 'TLS_AES_256_GCM_SHA384');
test('TLS_AES_256_GCM_SHA384', U, 'TLS_AES_256_GCM_SHA384');

// Do not have shared ciphers.
test('TLS_AES_256_GCM_SHA384', 'TLS_CHACHA20_POLY1305_SHA256',
     U, 'ECONNRESET', 'ERR_SSL_NO_SHARED_CIPHER');

test('AES128-SHA', 'AES256-SHA', U, 'ECONNRESET', 'ERR_SSL_NO_SHARED_CIPHER');
test('AES128-SHA:TLS_AES_256_GCM_SHA384',
     'TLS_CHACHA20_POLY1305_SHA256:AES256-SHA',
     U, 'ECONNRESET', 'ERR_SSL_NO_SHARED_CIPHER');

// Cipher order ignored, TLS1.3 chosen before TLS1.2.
test('AES256-SHA:TLS_AES_256_GCM_SHA384', U, 'TLS_AES_256_GCM_SHA384');
test(U, 'AES256-SHA:TLS_AES_256_GCM_SHA384', 'TLS_AES_256_GCM_SHA384');

// TLS_AES_128_CCM_8_SHA256 & TLS_AES_128_CCM_SHA256 are not enabled by
// default, but work.
test('TLS_AES_128_CCM_8_SHA256', U,
     U, 'ECONNRESET', 'ERR_SSL_NO_SHARED_CIPHER');

test('TLS_AES_128_CCM_8_SHA256', 'TLS_AES_128_CCM_8_SHA256',
     'TLS_AES_128_CCM_8_SHA256');

// Invalid cipher values
test(9, 'AES256-SHA', U, 'ERR_INVALID_ARG_TYPE', U);
test('AES256-SHA', 9, U, U, 'ERR_INVALID_ARG_TYPE');
test(':', 'AES256-SHA', U, 'ERR_INVALID_OPT_VALUE', U);
test('AES256-SHA', ':', U, U, 'ERR_INVALID_OPT_VALUE');

// Using '' is synonymous for "use default ciphers"
test('TLS_AES_256_GCM_SHA384', '', 'TLS_AES_256_GCM_SHA384');
test('', 'TLS_AES_256_GCM_SHA384', 'TLS_AES_256_GCM_SHA384');
