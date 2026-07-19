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

// Repeated requests for a domain that fails to resolve
// should trigger the error event after each attempt.

const common = require('../common');
const assert = require('assert');
const http = require('http');

function httpreq(count) {
  if (count > 1) return;

  const req = http.request({
    host: 'not-a-real-domain-name.nobody-would-register-this-as-a-tld',
    port: 80,
    path: '/',
    method: 'GET',
  }, common.mustNotCall());

  req.on('error', common.mustCall((e) => {
    assert.strictEqual(e.code, 'ENOTFOUND');
    httpreq(count + 1);
  }));

  req.end();
}

httpreq(0);
