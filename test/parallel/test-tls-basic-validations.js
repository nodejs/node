'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');

common.expectsError(
  () => tls.createSecureContext({ ciphers: 1 }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'Ciphers must be a string'
  });

common.expectsError(
  () => tls.createServer({ ciphers: 1 }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'Ciphers must be a string'
  });

common.expectsError(
  () => tls.createSecureContext({ key: 'dummykey', passphrase: 1 }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'Pass phrase must be a string'
  });

common.expectsError(
  () => tls.createServer({ key: 'dummykey', passphrase: 1 }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'Pass phrase must be a string'
  });

common.expectsError(
  () => tls.createServer({ ecdhCurve: 1 }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'ECDH curve name must be a string'
  });

common.expectsError(
  () => tls.createServer({ handshakeTimeout: 'abcd' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "options.handshakeTimeout" property must ' +
              'be of type number. Received type string'
  }
);

common.expectsError(
  () => tls.createServer({ sessionTimeout: 'abcd' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'Session timeout must be a 32-bit integer'
  });

common.expectsError(
  () => tls.createServer({ ticketKeys: 'abcd' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'Ticket keys must be a buffer'
  });

assert.throws(() => tls.createServer({ ticketKeys: Buffer.alloc(0) }),
              /TypeError: Ticket keys length must be 48 bytes/);

common.expectsError(
  () => tls.createSecurePair({}),
  {
    code: 'ERR_ASSERTION',
    message: 'context.context must be a NativeSecureContext'
  }
);

{
  const buffer = Buffer.from('abcd');
  const out = {};
  tls.convertALPNProtocols(buffer, out);
  out.ALPNProtocols.write('efgh');
  assert(buffer.equals(Buffer.from('abcd')));
  assert(out.ALPNProtocols.equals(Buffer.from('efgh')));
}

{
  const arrayBufferViewStr = 'abcd';
  const inputBuffer = Buffer.from(arrayBufferViewStr.repeat(8), 'utf8');
  for (const expectView of common.getArrayBufferViews(inputBuffer)) {
    const out = {};
    tls.convertALPNProtocols(expectView, out);
    assert(out.ALPNProtocols.equals(Buffer.from(expectView)));
  }
}

{
  const protocols = [(new String('a')).repeat(500)];
  const out = {};
  common.expectsError(
    () => tls.convertALPNProtocols(protocols, out),
    {
      code: 'ERR_OUT_OF_RANGE',
      message: 'The byte length of the protocol at index 0 exceeds the ' +
        'maximum length. It must be <= 255. Received 500'
    }
  );
}
