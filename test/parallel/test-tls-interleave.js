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

const options = { key: fixtures.readKey('rsa_private.pem'),
                  cert: fixtures.readKey('rsa_cert.crt'),
                  ca: [ fixtures.readKey('rsa_ca.crt') ] };

const writes = [
  'some server data',
  'and a separate packet',
  'and one more',
];
let receivedWrites = 0;

const server = tls.createServer(options, function(c) {
  c.resume();
  writes.forEach(function(str) {
    c.write(str);
  });
}).listen(0, common.mustCall(function() {
  const connectOpts = { rejectUnauthorized: false };
  const c = tls.connect(this.address().port, connectOpts, function() {
    c.write('some client data');
    c.on('readable', function() {
      let data = c.read();
      if (data === null)
        return;

      data = data.toString();
      while (data.length !== 0) {
        assert(data.startsWith(writes[receivedWrites]));
        data = data.slice(writes[receivedWrites].length);

        if (++receivedWrites === writes.length) {
          c.end();
          server.close();
        }
      }
    });
  });
}));


process.on('exit', function() {
  assert.strictEqual(receivedWrites, writes.length);
});
