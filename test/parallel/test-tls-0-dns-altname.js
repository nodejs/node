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
const assert = require('assert');

// Check getPeerCertificate can properly handle '\0' for fix CVE-2009-2408.

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');

const server = tls.createServer({
  key: fs.readFileSync(common.fixturesDir + '/0-dns/0-dns-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/0-dns/0-dns-cert.pem')
}, function(c) {
  c.once('data', function() {
    c.destroy();
    server.close();
  });
}).listen(0, common.mustCall(function() {
  const c = tls.connect(this.address().port, {
    rejectUnauthorized: false
  }, common.mustCall(function() {
    const cert = c.getPeerCertificate();
    assert.strictEqual(cert.subjectaltname,
                       'DNS:good.example.org\0.evil.example.com, ' +
                           'DNS:just-another.example.com, ' +
                           'IP Address:8.8.8.8, ' +
                           'IP Address:8.8.4.4, ' +
                           'DNS:last.example.com');
    c.write('ok');
  }));
}));
