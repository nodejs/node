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

// Omitting the cert or pfx option to tls.createServer() should not throw.
// AECDH-NULL-SHA is a no-authentication/no-encryption cipher and hence
// doesn't need a certificate.
tls.createServer({ ciphers: 'AECDH-NULL-SHA' })
  .listen(0, common.mustCall(close));

tls.createServer(assert.fail)
  .listen(0, common.mustCall(close));

tls.createServer({})
  .listen(0, common.mustCall(close));

common.expectsError(() => tls.createServer('this is not valid'),
                    {
                      code: 'ERR_INVALID_ARG_TYPE',
                      type: TypeError,
                      message: 'The "options" argument must be of type ' +
                               'Object. Received type string'
                    }
);

tls.createServer()
  .listen(0, common.mustCall(close));

function close() {
  this.close();
}
