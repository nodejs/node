'use strict';

const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const assert = require('assert');
const tls = require('tls');

assert.throws(() => { return tls.createSecureContext({ciphers: 1}); },
              /TypeError: Ciphers must be a string/);

assert.throws(() => { return tls.createServer({ciphers: 1}); },
              /TypeError: Ciphers must be a string/);

assert.throws(
  () => { return tls.createSecureContext({key: 'dummykey', passphrase: 1}); },
  /TypeError: Pass phrase must be a string/
);

assert.throws(
  () => { return tls.createServer({key: 'dummykey', passphrase: 1}); },
  /TypeError: Pass phrase must be a string/
);

assert.throws(() => { return tls.createServer({ecdhCurve: 1}); },
              /TypeError: ECDH curve name must be a string/);

assert.throws(() => { return tls.createServer({handshakeTimeout: 'abcd'}); },
              /TypeError: handshakeTimeout must be a number/);

assert.throws(() => { return tls.createServer({sessionTimeout: 'abcd'}); },
              /TypeError: Session timeout must be a 32-bit integer/);

assert.throws(() => { return tls.createServer({ticketKeys: 'abcd'}); },
              /TypeError: Ticket keys must be a buffer/);

assert.throws(() => { return tls.createServer({ticketKeys: new Buffer(0)}); },
              /TypeError: Ticket keys length must be 48 bytes/);

assert.throws(() => { return tls.createSecurePair({}); },
              /Error: First argument must be a tls module SecureContext/);

{
  const buffer = Buffer.from('abcd');
  const out = {};
  tls.convertALPNProtocols(buffer, out);
  out.ALPNProtocols.write('efgh');
  assert(buffer.equals(Buffer.from('abcd')));
  assert(out.ALPNProtocols.equals(Buffer.from('efgh')));
}

{
  const buffer = Buffer.from('abcd');
  const out = {};
  tls.convertNPNProtocols(buffer, out);
  out.NPNProtocols.write('efgh');
  assert(buffer.equals(Buffer.from('abcd')));
  assert(out.NPNProtocols.equals(Buffer.from('efgh')));
}
