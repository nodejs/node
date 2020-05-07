// Flags: --no-warnings
'use strict';

// Tests error and input validation checks for QuicSocket.connect()

const common = require('../common');

if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const { createQuicSocket } = require('net');

// Test invalid callback function
{
  const server = createQuicSocket();
  [1, 1n].forEach((cb) => {
    assert.throws(() => server.listen({}, cb), {
      code: 'ERR_INVALID_CALLBACK'
    });
  });
}

// Test QuicSocket is already listening
{
  const server = createQuicSocket();
  server.listen();
  assert.throws(() => server.listen(), {
    code: 'ERR_QUICSOCKET_LISTENING'
  });
  server.close();
}

// Test QuicSocket listen after destroy error
{
  const server = createQuicSocket();
  server.close();
  assert.throws(() => server.listen(), {
    code: 'ERR_QUICSOCKET_DESTROYED'
  });
}

{
  // Test incorrect ALPN
  const server = createQuicSocket();
  [1, 1n, true, {}, [], null].forEach((alpn) => {
    assert.throws(() => server.listen({ alpn }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  });

  // Test invalid idle timeout
  [
    'idleTimeout',
    'activeConnectionIdLimit',
    'maxAckDelay',
    'maxData',
    'maxPacketSize',
    'maxStreamDataBidiLocal',
    'maxStreamDataBidiRemote',
    'maxStreamDataUni',
    'maxStreamsBidi',
    'maxStreamsUni',
    'highWaterMark',
  ].forEach((prop) => {
    assert.throws(() => server.listen({ [prop]: -1 }), {
      code: 'ERR_OUT_OF_RANGE'
    });

    assert.throws(
      () => server.listen({ [prop]: Number.MAX_SAFE_INTEGER + 1 }), {
        code: 'ERR_OUT_OF_RANGE'
      });

    ['a', 1n, [], {}, false].forEach((val) => {
      assert.throws(() => server.listen({ [prop]: val }), {
        code: 'ERR_INVALID_ARG_TYPE'
      });
    });
  });

  [1, 1n, 'test', {}, []].forEach((rejectUnauthorized) => {
    assert.throws(() => server.listen({ rejectUnauthorized }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  });

  [1, 1n, 'test', {}, []].forEach((requestCert) => {
    assert.throws(() => server.listen({ requestCert }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  });

  [1, 1n, 'test', {}, []].forEach((requestCert) => {
    assert.throws(() => server.listen({ requestCert }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  });

  [1, 1n, 'test', false].forEach((preferredAddress) => {
    assert.throws(() => server.listen({ preferredAddress }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });

    [1, 1n, null, false, {}, []].forEach((address) => {
      assert.throws(() => server.listen({ preferredAddress: { address } }), {
        code: 'ERR_INVALID_ARG_TYPE'
      });
    });

    [-1].forEach((port) => {
      assert.throws(() => server.listen({ preferredAddress: { port } }), {
        code: 'ERR_INVALID_ARG_TYPE'
      });
    });

    [1, 'test', false, null, {}, []].forEach((type) => {
      assert.throws(() => server.listen({ preferredAddress: { type } }), {
        code: 'ERR_INVALID_ARG_TYPE'
      });
    });
  });

  [1, 1n, false, [], {}, null].forEach((ciphers) => {
    assert.throws(() => server.listen({ ciphers }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  });

  [1, 1n, false, [], {}, null].forEach((groups) => {
    assert.throws(() => server.listen({ groups }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  });

  ['', 1n, {}, [], false, 'zebra'].forEach((defaultEncoding) => {
    assert.throws(() => server.listen({ defaultEncoding }), {
      code: 'ERR_INVALID_ARG_VALUE'
    });
  });

  // Make sure that after all of the validation checks, the socket
  // is not actually marked as listening at all.
  assert.strictEqual(typeof server.listening, 'boolean');
  assert(!server.listening);
}


// Options to check
// * [x] alpn
// * [x] idleTimeout
// * [x] activeConnectionIdLimit
// * [x] maxAckDelay
// * [x] maxData
// * [x] maxPacketSize
// * [x] maxStreamsBidi
// * [x] maxStreamsUni
// * [x] maxStreamDataBidiLocal
// * [x] maxStreamDataBidiRemote
// * [x] maxStreamDataUni
// * [x] preferredAddress
// * [x] requestCert
// * [x] rejectUnauthorized

// SecureContext Options
// * [ ] ca
// * [ ] cert
// * [x] ciphers
// * [ ] clientCertEngine
// * [ ] crl
// * [ ] dhparam
// * [ ] groups
// * [ ] ecdhCurve
// * [ ] honorCipherOrder
// * [ ] key
// * [ ] passphrase
// * [ ] pfx
// * [ ] secureOptions
// * [ ] sessionIdContext
