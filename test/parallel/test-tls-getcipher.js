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
// Import fixtures directly from its module
const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem'),
  honorCipherOrder: true
};

const isBoringSSL = process.features.openssl_is_boringssl;
let clients = 0;
const expectedClients = isBoringSSL ? 1 : 2;
const server = tls.createServer(options, common.mustCall(() => {
  if (--clients === 0)
    server.close();
}, expectedClients));

server.listen(0, '127.0.0.1', common.mustCall(function() {
  if (isBoringSSL) {
    // BoringSSL does not provide this static RSA TLS 1.2 cipher suite on
    // Node's supported cipher surface, so keep the OpenSSL getCipher()
    // assertion below limited to backends that can create the context.
    common.printSkipMessage('BoringSSL does not provide AES256-SHA256');
    assert.throws(() => tls.createSecureContext({ ciphers: 'AES256-SHA256' }), {
      code: 'ERR_SSL_NO_CIPHER_MATCH',
      library: 'SSL routines',
      function: 'OPENSSL_internal',
      reason: 'NO_CIPHER_MATCH',
    });
  } else {
    clients++;
    tls.connect({
      host: '127.0.0.1',
      port: this.address().port,
      ciphers: 'AES256-SHA256',
      rejectUnauthorized: false,
      maxVersion: 'TLSv1.2',
    }, common.mustCall(function() {
      const cipher = this.getCipher();
      assert.strictEqual(cipher.name, 'AES256-SHA256');
      assert.strictEqual(cipher.standardName, 'TLS_RSA_WITH_AES_256_CBC_SHA256');
      assert.strictEqual(cipher.version, 'TLSv1.2');
      this.end();
    }));
  }

  clients++;
  tls.connect({
    host: '127.0.0.1',
    port: this.address().port,
    ciphers: 'ECDHE-RSA-AES256-GCM-SHA384',
    rejectUnauthorized: false,
    maxVersion: 'TLSv1.2',
  }, common.mustCall(function() {
    const cipher = this.getCipher();
    assert.strictEqual(cipher.name, 'ECDHE-RSA-AES256-GCM-SHA384');
    assert.strictEqual(cipher.standardName,
                       'TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384');
    assert.strictEqual(cipher.version, isBoringSSL ?
      'TLSv1/SSLv3' :
      'TLSv1.2');
    this.end();
  }));
}));

tls.createServer({
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem'),
  ciphers: 'TLS_CHACHA20_POLY1305_SHA256:TLS_AES_256_GCM_SHA384',
  maxVersion: 'TLSv1.3',
}, common.mustCall(function() {
  this.close();
})).listen(0, common.mustCall(function() {
  const client = tls.connect({
    port: this.address().port,
    ciphers: 'TLS_AES_256_GCM_SHA384',
    maxVersion: 'TLSv1.3',
    rejectUnauthorized: false
  }, common.mustCall(() => {
    const cipher = client.getCipher();
    const expectedCipher = isBoringSSL ?
      'TLS_AES_128_GCM_SHA256' :
      'TLS_AES_256_GCM_SHA384';
    assert.strictEqual(cipher.name, expectedCipher);
    assert.strictEqual(cipher.standardName, cipher.name);
    assert.strictEqual(cipher.version, isBoringSSL ?
      'TLSv1/SSLv3' :
      'TLSv1.3');
    client.end();
  }));
}));
