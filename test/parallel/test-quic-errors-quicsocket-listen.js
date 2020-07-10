// Flags: --no-warnings
'use strict';

// Tests error and input validation checks for QuicSocket.connect()

const common = require('../common');

if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const { createQuicSocket } = require('net');

async function testAlreadyListening() {
  const server = createQuicSocket();
  // Can be called multiple times while pending...
  await Promise.all([server.listen(), server.listen()]);
  // But fails if called again after resolving
  await assert.rejects(server.listen(), {
    code: 'ERR_INVALID_STATE'
  });
  server.close();
}

async function testListenAfterClose() {
  const server = createQuicSocket();
  server.close();
  await assert.rejects(server.listen(), {
    code: 'ERR_INVALID_STATE'
  });
}

async function rejectsValue(
  server,
  name,
  values,
  code = 'ERR_INVALID_ARG_TYPE') {
  for (const v of values) {
    await assert.rejects(server.listen({ [name]: v }), { code });
  }
}

async function testInvalidOptions() {
  const server = createQuicSocket();

  await rejectsValue(
    server,
    'alpn',
    [1, 1n, true, {}, [], null]);

  for (const prop of [
    'idleTimeout',
    'activeConnectionIdLimit',
    'maxAckDelay',
    'maxData',
    'maxUdpPayloadSize',
    'maxStreamDataBidiLocal',
    'maxStreamDataBidiRemote',
    'maxStreamDataUni',
    'maxStreamsBidi',
    'maxStreamsUni',
    'highWaterMark',
  ]) {
    await rejectsValue(
      server,
      prop,
      [-1],
      'ERR_OUT_OF_RANGE');
    await rejectsValue(
      server,
      prop,
      [Number.MAX_SAFE_INTEGER + 1],
      'ERR_OUT_OF_RANGE');
    await rejectsValue(
      server,
      prop,
      ['a', 1n, [], {}, false]);
  }

  await rejectsValue(
    server,
    'rejectUnauthorized',
    [1, 1n, 'test', {}, []]);

  await rejectsValue(
    server,
    'requestCert',
    [1, 1n, 'test', {}, []]);

  await rejectsValue(
    server,
    'ciphers',
    [1, 1n, false, {}, [], null]);

  await rejectsValue(
    server,
    'groups',
    [1, 1n, false, {}, [], null]);

  await rejectsValue(
    server,
    'defaultEncoding',
    [1, 1n, false, {}, [], 'zebra'],
    'ERR_INVALID_ARG_VALUE');

  await rejectsValue(
    server,
    'preferredAddress',
    [1, 1n, 'test', false]
  );

  await assert.rejects(
    server.listen({ preferredAddress: { port: -1 } }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });

  for (const address of [1, 1n, null, false, [], {}]) {
    await assert.rejects(
      server.listen({ preferredAddress: { address } }), {
        code: 'ERR_INVALID_ARG_TYPE'
      });
  }

  for (const type of [1, 'test', false, null, [], {}]) {
    await assert.rejects(
      server.listen({ preferredAddress: { type } }), {
        code: 'ERR_INVALID_ARG_TYPE'
      });
  }

  // Make sure that after all of the validation checks, the socket
  // is not actually marked as listening at all.
  assert.strictEqual(typeof server.listening, 'boolean');
  assert(!server.listening);
}

(async function() {
  await testAlreadyListening();
  await testListenAfterClose();
  await testInvalidOptions();
})().then(common.mustCall());

// Options to check
// * [x] alpn
// * [x] idleTimeout
// * [x] activeConnectionIdLimit
// * [x] maxAckDelay
// * [x] maxData
// * [x] maxUdpPayloadSize
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
