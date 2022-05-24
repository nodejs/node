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
  });

assert.throws(
  () => tls.createServer({ ciphers: 1 }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  });

assert.throws(
  () => tls.createSecureContext({ key: 'dummykey', passphrase: 1 }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  });

assert.throws(
  () => tls.createServer({ key: 'dummykey', passphrase: 1 }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  });

assert.throws(
  () => tls.createServer({ ecdhCurve: 1 }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  });

assert.throws(
  () => tls.createServer({ handshakeTimeout: 'abcd' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  }
);

assert.throws(
  () => tls.createServer({ sessionTimeout: 'abcd' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  });

assert.throws(
  () => tls.createServer({ ticketKeys: 'abcd' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  });

assert.throws(() => tls.createServer({ ticketKeys: Buffer.alloc(0) }), {
  code: 'ERR_INVALID_ARG_VALUE',
});

assert.throws(
  () => tls.createSecurePair({}),
  {
    code: 'ERR_TLS_INVALID_CONTEXT',
    name: 'TypeError',
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
