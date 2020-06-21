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

// Test invalid minDHSize options argument
['test', 1n, {}, [], false].forEach((minDHSize) => {
  assert.throws(() => client.connect({ minDHSize }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid port argument option
[-1, 'test', 1n, {}, [], NaN, false, 65536].forEach((port) => {
  assert.throws(() => client.connect({ port }), {
    code: 'ERR_SOCKET_BAD_PORT'
  });
});

// Test invalid address argument option
[-1, 10, 1n, {}, [], true].forEach((address) => {
  assert.throws(() => client.connect({ address }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test servername can't be IP address argument option
[
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
].forEach((servername) => {
  assert.throws(() => client.connect({ servername }), {
    code: 'ERR_INVALID_ARG_VALUE'
  });
});

[-1, 10, 1n, {}, [], true].forEach((servername) => {
  assert.throws(() => client.connect({ servername }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid remoteTransportParams argument option
[-1, 'test', 1n, {}, []].forEach((remoteTransportParams) => {
  assert.throws(() => client.connect({ remoteTransportParams }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid sessionTicket argument option
[-1, 'test', 1n, {}, []].forEach((sessionTicket) => {
  assert.throws(() => client.connect({ sessionTicket }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid alpn argument option
[-1, 10, 1n, {}, [], true].forEach((alpn) => {
  assert.throws(() => client.connect({ alpn }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

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
  assert.throws(() => client.connect({ [prop]: -1 }), {
    code: 'ERR_OUT_OF_RANGE'
  });

  assert.throws(
    () => client.connect({ [prop]: Number.MAX_SAFE_INTEGER + 1 }), {
      code: 'ERR_OUT_OF_RANGE'
    });

  ['a', 1n, [], {}, false].forEach((val) => {
    assert.throws(() => client.connect({ [prop]: val }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  });
});

// activeConnectionIdLimit must be between 2 and 8, inclusive
[1, 9].forEach((activeConnectionIdLimit) => {
  assert.throws(() => client.connect({ activeConnectionIdLimit }), {
    code: 'ERR_OUT_OF_RANGE'
  });
});

['a', 1n, 1, [], {}].forEach((ipv6Only) => {
  assert.throws(() => client.connect({ ipv6Only }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

[1, 1n, false, [], {}].forEach((preferredAddressPolicy) => {
  assert.throws(() => client.connect({ preferredAddressPolicy }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

[1, 1n, 'test', [], {}].forEach((qlog) => {
  assert.throws(() => client.connect({ qlog }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

[1, 1n, 'test', [], {}].forEach((requestOCSP) => {
  assert.throws(() => client.connect({ requestOCSP }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

[1, 1n, false, [], {}, 'aaa'].forEach((type) => {
  assert.throws(() => client.connect({ type }), {
    code: 'ERR_INVALID_ARG_VALUE'
  });
});


[
  'qpackMaxTableCapacity',
  'qpackBlockedStreams',
  'maxHeaderListSize',
  'maxPushes',
].forEach((prop) => {
  assert.throws(() => client.connect({ h3: { [prop]: -1 } }), {
    code: 'ERR_OUT_OF_RANGE'
  });

  assert.throws(
    () => client.connect({ h3: { [prop]: Number.MAX_SAFE_INTEGER + 1 } }), {
      code: 'ERR_OUT_OF_RANGE'
    });

  ['a', 1n, [], {}, false].forEach((val) => {
    assert.throws(() => client.connect({ h3: { [prop]: val } }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  });
});

['', 1n, {}, [], false, 'zebra'].forEach((defaultEncoding) => {
  assert.throws(() => client.connect({ defaultEncoding }), {
    code: 'ERR_INVALID_ARG_VALUE'
  });
});


// Test that connect cannot be called after QuicSocket is closed.
client.close();
assert.throws(() => client.connect(), {
  code: 'ERR_QUICSOCKET_DESTROYED'
});

// TODO(@jasnell): Test additional options:
//
// Client QuicSession Related:
//
//  [x] idleTimeout - must be a number greater than zero
//  [x] ipv6Only - must be a boolean
//  [x] activeConnectionIdLimit - must be a number between 2 and 8
//  [x] maxAckDelay - must be a number greater than zero
//  [x] maxData - must be a number greater than zero
//  [x] maxPacketSize - must be a number greater than zero
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
