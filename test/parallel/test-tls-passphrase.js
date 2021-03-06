// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const passKey = fixtures.readKey('rsa_private_encrypted.pem');
const rawKey = fixtures.readKey('rsa_private.pem');
const cert = fixtures.readKey('rsa_cert.crt');

assert(Buffer.isBuffer(passKey));
assert(Buffer.isBuffer(cert));
assert.strictEqual(typeof passKey.toString(), 'string');
assert.strictEqual(typeof cert.toString(), 'string');

function onSecureConnect() {
  return common.mustCall(function() { this.end(); });
}

const server = tls.Server({
  key: passKey,
  passphrase: 'password',
  cert: cert,
  ca: [cert],
  requestCert: true,
  rejectUnauthorized: true
});

server.listen(0, common.mustCall(function() {
  // Buffer
  tls.connect({
    port: this.address().port,
    key: passKey,
    passphrase: 'password',
    cert: cert,
    rejectUnauthorized: false
  }, onSecureConnect());

  tls.connect({
    port: this.address().port,
    key: rawKey,
    cert: cert,
    rejectUnauthorized: false
  }, onSecureConnect());

  tls.connect({
    port: this.address().port,
    key: rawKey,
    passphrase: 'ignored',
    cert: cert,
    rejectUnauthorized: false
  }, onSecureConnect());

  // Buffer[]
  tls.connect({
    port: this.address().port,
    key: [passKey],
    passphrase: 'password',
    cert: [cert],
    rejectUnauthorized: false
  }, onSecureConnect());

  tls.connect({
    port: this.address().port,
    key: [rawKey],
    cert: [cert],
    rejectUnauthorized: false
  }, onSecureConnect());

  tls.connect({
    port: this.address().port,
    key: [rawKey],
    passphrase: 'ignored',
    cert: [cert],
    rejectUnauthorized: false
  }, onSecureConnect());

  // string
  tls.connect({
    port: this.address().port,
    key: passKey.toString(),
    passphrase: 'password',
    cert: cert.toString(),
    rejectUnauthorized: false
  }, onSecureConnect());

  tls.connect({
    port: this.address().port,
    key: rawKey.toString(),
    cert: cert.toString(),
    rejectUnauthorized: false
  }, onSecureConnect());

  tls.connect({
    port: this.address().port,
    key: rawKey.toString(),
    passphrase: 'ignored',
    cert: cert.toString(),
    rejectUnauthorized: false
  }, onSecureConnect());

  // String[]
  tls.connect({
    port: this.address().port,
    key: [passKey.toString()],
    passphrase: 'password',
    cert: [cert.toString()],
    rejectUnauthorized: false
  }, onSecureConnect());

  tls.connect({
    port: this.address().port,
    key: [rawKey.toString()],
    cert: [cert.toString()],
    rejectUnauthorized: false
  }, onSecureConnect());

  tls.connect({
    port: this.address().port,
    key: [rawKey.toString()],
    passphrase: 'ignored',
    cert: [cert.toString()],
    rejectUnauthorized: false
  }, onSecureConnect());

  // Object[]
  tls.connect({
    port: this.address().port,
    key: [{ pem: passKey, passphrase: 'password' }],
    cert: cert,
    rejectUnauthorized: false
  }, onSecureConnect());

  tls.connect({
    port: this.address().port,
    key: [{ pem: passKey, passphrase: 'password' }],
    passphrase: 'ignored',
    cert: cert,
    rejectUnauthorized: false
  }, onSecureConnect());

  tls.connect({
    port: this.address().port,
    key: [{ pem: passKey }],
    passphrase: 'password',
    cert: cert,
    rejectUnauthorized: false
  }, onSecureConnect());

  tls.connect({
    port: this.address().port,
    key: [{ pem: passKey.toString(), passphrase: 'password' }],
    cert: cert,
    rejectUnauthorized: false
  }, onSecureConnect());

  tls.connect({
    port: this.address().port,
    key: [{ pem: rawKey, passphrase: 'ignored' }],
    cert: cert,
    rejectUnauthorized: false
  }, onSecureConnect());

  tls.connect({
    port: this.address().port,
    key: [{ pem: rawKey.toString(), passphrase: 'ignored' }],
    cert: cert,
    rejectUnauthorized: false
  }, onSecureConnect());

  tls.connect({
    port: this.address().port,
    key: [{ pem: rawKey }],
    passphrase: 'ignored',
    cert: cert,
    rejectUnauthorized: false
  }, onSecureConnect());

  tls.connect({
    port: this.address().port,
    key: [{ pem: rawKey.toString() }],
    passphrase: 'ignored',
    cert: cert,
    rejectUnauthorized: false
  }, onSecureConnect());

  tls.connect({
    port: this.address().port,
    key: [{ pem: rawKey }],
    cert: cert,
    rejectUnauthorized: false
  }, onSecureConnect());

  tls.connect({
    port: this.address().port,
    key: [{ pem: rawKey.toString() }],
    cert: cert,
    rejectUnauthorized: false
  }, onSecureConnect());
})).unref();

const errMessagePassword = /bad decrypt/;

// Missing passphrase
assert.throws(function() {
  tls.connect({
    port: server.address().port,
    key: passKey,
    cert: cert,
    rejectUnauthorized: false
  });
}, errMessagePassword);

assert.throws(function() {
  tls.connect({
    port: server.address().port,
    key: [passKey],
    cert: cert,
    rejectUnauthorized: false
  });
}, errMessagePassword);

assert.throws(function() {
  tls.connect({
    port: server.address().port,
    key: [{ pem: passKey }],
    cert: cert,
    rejectUnauthorized: false
  });
}, errMessagePassword);

const errMessageDecrypt = /bad decrypt/;

// Invalid passphrase
assert.throws(function() {
  tls.connect({
    port: server.address().port,
    key: passKey,
    passphrase: 'invalid',
    cert: cert,
    rejectUnauthorized: false
  });
}, errMessageDecrypt);

assert.throws(function() {
  tls.connect({
    port: server.address().port,
    key: [passKey],
    passphrase: 'invalid',
    cert: cert,
    rejectUnauthorized: false
  });
}, errMessageDecrypt);

assert.throws(function() {
  tls.connect({
    port: server.address().port,
    key: [{ pem: passKey }],
    passphrase: 'invalid',
    cert: cert,
    rejectUnauthorized: false
  });
}, errMessageDecrypt);

assert.throws(function() {
  tls.connect({
    port: server.address().port,
    key: [{ pem: passKey, passphrase: 'invalid' }],
    passphrase: 'password', // Valid but unused
    cert: cert,
    rejectUnauthorized: false
  });
}, errMessageDecrypt);
