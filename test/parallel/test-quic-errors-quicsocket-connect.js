// Flags: --no-warnings
'use strict';

// Tests error and input validation checks for QuicSocket.connect()

const common = require('../common');

if (!common.hasQuic)
  common.skip('missing quic');

const { createHook } = require('async_hooks');
const assert = require('assert');
const { createQuicSocket } = require('net');

// Ensure that a QuicClientSession handle is never created during the
// error condition tests (ensures that argument and error validation)
// is occurring before the underlying handle is created.
createHook({
  init(id, type) {
    assert.notStrictEqual(type, 'QUICCLIENTSESSION');
  }
}).enable();

const client = createQuicSocket();

(async function() {
  await Promise.all(['test', 1n, {}, [], false].map((minDHSize) => {
    return assert.rejects(client.connect({ minDHSize }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  }));

  await Promise.all([-1, 'test', 1n, {}, [], NaN, false, 65536].map((port) => {
    return assert.rejects(client.connect({ port }), {
      code: 'ERR_SOCKET_BAD_PORT'
    });
  }));

  // Test invalid address argument option
  await Promise.all([-1, 10, 1n, {}, [], true].map((address) => {
    return assert.rejects(client.connect({ address }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  }));

  // Test servername can't be IP address argument option
  await Promise.all([
    '0.0.0.0',
    '8.8.8.8',
    '127.0.0.1',
    '192.168.0.1',
    '::',
    '1::',
    '::1',
    '1::8',
    '1::7:8',
    '1:2:3:4:5:6:7:8',
    '1:2:3:4:5:6::8',
    '2001:0000:1234:0000:0000:C1C0:ABCD:0876',
    '3ffe:0b00:0000:0000:0001:0000:0000:000a',
    'a:0:0:0:0:0:0:0',
    'fe80::7:8%eth0',
    'fe80::7:8%1'
  ].map((servername) => {
    return assert.rejects(client.connect({ servername }), {
      code: 'ERR_INVALID_ARG_VALUE'
    });
  }));

  await Promise.all([-1, 10, 1n, {}, [], true].map((servername) => {
    return assert.rejects(client.connect({ servername }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  }));

  // Test invalid remoteTransportParams argument option
  await Promise.all([-1, 'test', 1n, {}, []].map((remoteTransportParams) => {
    return assert.rejects(client.connect({ remoteTransportParams }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  }));

  // Test invalid sessionTicket argument option
  await Promise.all([-1, 'test', 1n, {}, []].map((sessionTicket) => {
    return assert.rejects(client.connect({ sessionTicket }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  }));

  // Test invalid alpn argument option
  await Promise.all([-1, 10, 1n, {}, [], true].map((alpn) => {
    return assert.rejects(client.connect({ alpn }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  }));

  await Promise.all([
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
  ].map(async (prop) => {
    await assert.rejects(client.connect({ [prop]: -1 }), {
      code: 'ERR_OUT_OF_RANGE'
    });

    await assert.rejects(
      client.connect({ [prop]: Number.MAX_SAFE_INTEGER + 1 }), {
        code: 'ERR_OUT_OF_RANGE'
      });

    await Promise.all(['a', 1n, [], {}, false].map((val) => {
      return assert.rejects(client.connect({ [prop]: val }), {
        code: 'ERR_INVALID_ARG_TYPE'
      });
    }));
  }));

  // activeConnectionIdLimit must be between 2 and 8, inclusive
  await Promise.all([1, 9].map((activeConnectionIdLimit) => {
    return assert.rejects(client.connect({ activeConnectionIdLimit }), {
      code: 'ERR_OUT_OF_RANGE'
    });
  }));

  await Promise.all([1, 1n, false, [], {}].map((preferredAddressPolicy) => {
    return assert.rejects(client.connect({ preferredAddressPolicy }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  }));

  await Promise.all([1, 1n, 'test', [], {}].map((qlog) => {
    return assert.rejects(client.connect({ qlog }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  }));

  await Promise.all([1, 1n, 'test', [], {}].map((ocspHandler) => {
    return assert.rejects(client.connect({ ocspHandler }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  }));

  await Promise.all([1, 1n, false, [], {}, 'aaa'].map((type) => {
    return assert.rejects(client.connect({ type }), {
      code: 'ERR_INVALID_ARG_VALUE'
    });
  }));

  await Promise.all([
    'qpackMaxTableCapacity',
    'qpackBlockedStreams',
    'maxHeaderListSize',
    'maxPushes',
  ].map(async (prop) => {
    await assert.rejects(client.connect({ h3: { [prop]: -1 } }), {
      code: 'ERR_OUT_OF_RANGE'
    });

    await assert.rejects(
      client.connect({ h3: { [prop]: Number.MAX_SAFE_INTEGER + 1 } }), {
        code: 'ERR_OUT_OF_RANGE'
      });

    await Promise.all(['a', 1n, [], {}, false].map((val) => {
      return assert.rejects(client.connect({ h3: { [prop]: val } }), {
        code: 'ERR_INVALID_ARG_TYPE'
      });
    }));
  }));

  await Promise.all(['', 1n, {}, [], false, 'zebra'].map((defaultEncoding) => {
    return assert.rejects(client.connect({ defaultEncoding }), {
      code: 'ERR_INVALID_ARG_VALUE'
    });
  }));

  // Test that connect cannot be called after QuicSocket is closed.
  client.close();

  await assert.rejects(client.connect(), {
    code: 'ERR_INVALID_STATE'
  });
})().then(common.mustCall());

// TODO(@jasnell): Test additional options:
//
// Client QuicSession Related:
//
//  [x] idleTimeout - must be a number greater than zero
//  [x] activeConnectionIdLimit - must be a number between 2 and 8
//  [x] maxAckDelay - must be a number greater than zero
//  [x] maxData - must be a number greater than zero
//  [x] maxUdpPayloadSize - must be a number greater than zero
//  [x] maxStreamDataBidiLocal - must be a number greater than zero
//  [x] maxStreamDataBidiRemote - must be a number greater than zero
//  [x] maxStreamDataUni - must be a number greater than zero
//  [x] maxStreamsBidi - must be a number greater than zero
//  [x] maxStreamsUni - must be a number greater than zero
//  [x] preferredAddressPolicy - must be eiher 'accept' or 'reject'
//  [x] qlog - must be a boolean
//  [x] requestOCSP - must be a boolean
//  [x] type - must be a string, either 'udp4' or 'udp6'
//
// HTTP/3 Related:
//
//  [x] h3.qpackMaxTableCapacity - must be a number greater than zero
//  [x] h3.qpackBlockedStreams - must be a number greater than zero
//  [x] h3.maxHeaderListSize - must be a number greater than zero
//  [x] h3.maxPushes - must be a number greater than zero
//
// Secure Context Related:
//
//  [ ] ca (certificate authority) - must be a string, string array,
//      Buffer, or Buffer array.
//  [ ] cert (cert chain) - must be a string, string array, Buffer, or
//      Buffer array.
//  [ ] ciphers - must be a string
//  [ ] clientCertEngine - must be a string
//  [ ] crl - must be a string, string array, Buffer, or Buffer array
//  [ ] dhparam - must be a string or Buffer
//  [ ] ecdhCurve - must be a string
//  [ ] honorCipherOrder - must be a boolean
//  [ ] key - must be a string, string array, Buffer, or Buffer array
//  [ ] passphrase - must be a string
//  [ ] pfx - must be a string, string array, Buffer, or Buffer array
//  [ ] secureOptions - must be a number
//  [x] minDHSize - must be a number
