'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');

assert.throws(
  () => tls.createSecureContext({ ciphers: 1 }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "options.ciphers" property must be of type string.' +
      ' Received type number (1)'
  });

assert.throws(
  () => tls.createServer({ ciphers: 1 }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "options.ciphers" property must be of type string.' +
      ' Received type number (1)'
  });

assert.throws(
  () => tls.createSecureContext({ key: 'dummykey', passphrase: 1 }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: /The "options\.passphrase" property must be of type string/
  });

assert.throws(
  () => tls.createServer({ key: 'dummykey', passphrase: 1 }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: /The "options\.passphrase" property must be of type string/
  });

assert.throws(
  () => tls.createServer({ ecdhCurve: 1 }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: /The "options\.ecdhCurve" property must be of type string/
  });

assert.throws(
  () => tls.createServer({ handshakeTimeout: 'abcd' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "options.handshakeTimeout" property must be of type number.' +
              " Received type string ('abcd')"
  }
);

assert.throws(
  () => tls.createServer({ sessionTimeout: 'abcd' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: /The "options\.sessionTimeout" property must be of type number/
  });

assert.throws(
  () => tls.createServer({ ticketKeys: 'abcd' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: /The "options\.ticketKeys" property must be an instance of/
  });

assert.throws(() => tls.createServer({ ticketKeys: Buffer.alloc(0) }), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /The property 'options\.ticketKeys' must be exactly 48 bytes/
});

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
    const expected = Buffer.from(expectView.buffer.slice(),
                                 expectView.byteOffset,
                                 expectView.byteLength);
    tls.convertALPNProtocols(expectView, out);
    assert(out.ALPNProtocols.equals(expected));
  }
}

{
  const protocols = [(new String('a')).repeat(500)];
  const out = {};
  assert.throws(
    () => tls.convertALPNProtocols(protocols, out),
    {
      code: 'ERR_OUT_OF_RANGE',
      message: 'The byte length of the protocol at index 0 exceeds the ' +
        'maximum length. It must be <= 255. Received 500'
    }
  );
}

assert.throws(() => { tls.createSecureContext({ minVersion: 'fhqwhgads' }); },
              {
                code: 'ERR_TLS_INVALID_PROTOCOL_VERSION',
                name: 'TypeError'
              });

assert.throws(() => { tls.createSecureContext({ maxVersion: 'fhqwhgads' }); },
              {
                code: 'ERR_TLS_INVALID_PROTOCOL_VERSION',
                name: 'TypeError'
              });

for (const checkServerIdentity of [undefined, null, 1, true]) {
  assert.throws(() => {
    tls.connect({ checkServerIdentity });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  });
}
