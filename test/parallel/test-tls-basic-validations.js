'use strict';

require('../common');
const assert = require('assert');
const tls = require('tls');

assert.throws(() => tls.createSecureContext({ciphers: 1}),
              /TypeError: Ciphers must be a string/);

assert.throws(() => tls.createServer({ciphers: 1}),
              /TypeError: Ciphers must be a string/);

assert.throws(() => tls.createSecureContext({key: 'dummykey', passphrase: 1}),
              /TypeError: Pass phrase must be a string/);

assert.throws(() => tls.createServer({key: 'dummykey', passphrase: 1}),
              /TypeError: Pass phrase must be a string/);

assert.throws(() => tls.createServer({ecdhCurve: 1}),
              /TypeError: ECDH curve name must be a string/);

assert.throws(() => tls.createServer({handshakeTimeout: 'abcd'}),
              /TypeError: handshakeTimeout must be a number/);

assert.throws(() => tls.createServer({sessionTimeout: 'abcd'}),
              /TypeError: Session timeout must be a 32-bit integer/);

assert.throws(() => tls.createServer({ticketKeys: 'abcd'}),
              /TypeError: Ticket keys must be a buffer/);

assert.throws(() => tls.createServer({ticketKeys: new Buffer(0)}),
              /TypeError: Ticket keys length must be 48 bytes/);

assert.throws(() => tls.createSecurePair({}),
              /Error: First argument must be a tls module SecureContext/);
