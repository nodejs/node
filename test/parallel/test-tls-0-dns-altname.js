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

// Check getPeerCertificate can properly handle '\0' for fix CVE-2009-2408.

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const server = tls.createServer({
  key: fixtures.readSync(['0-dns', '0-dns-key.pem']),
  cert: fixtures.readSync(['0-dns', '0-dns-cert.pem'])
}, common.mustCall((c) => {
  c.once('data', common.mustCall(() => {
    c.destroy();
    server.close();
  }));
})).listen(0, common.mustCall(() => {
  const c = tls.connect(server.address().port, {
    rejectUnauthorized: false
  }, common.mustCall(() => {
    const cert = c.getPeerCertificate();
    assert.strictEqual(cert.subjectaltname,
                       'DNS:"good.example.org\\u0000.evil.example.com", ' +
                           'DNS:just-another.example.com, ' +
                           'IP Address:8.8.8.8, ' +
                           'IP Address:8.8.4.4, ' +
                           'DNS:last.example.com');
    c.write('ok');
  }));
}));
