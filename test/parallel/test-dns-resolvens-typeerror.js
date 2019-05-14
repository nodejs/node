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

// This test ensures `dns.resolveNs()` does not raise a C++-land assertion error
// and throw a JavaScript TypeError instead.
// Issue https://github.com/nodejs/node-v0.x-archive/issues/7070

const dns = require('dns');
const dnsPromises = dns.promises;

common.expectsError(
  () => dnsPromises.resolveNs([]), // bad name
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: /^The "name" argument must be of type string/
  }
);
common.expectsError(
  () => dns.resolveNs([]), // bad name
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: /^The "name" argument must be of type string/
  }
);
common.expectsError(
  () => dns.resolveNs(''), // bad callback
  {
    code: 'ERR_INVALID_CALLBACK',
    type: TypeError
  }
);
