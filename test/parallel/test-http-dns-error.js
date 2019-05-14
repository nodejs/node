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
const http = require('http');
const https = require('https');

const host = '*'.repeat(64);
const MAX_TRIES = 5;

const errCodes = ['ENOTFOUND', 'EAI_FAIL'];

function tryGet(mod, tries) {
  // Bad host name should not throw an uncatchable exception.
  // Ensure that there is time to attach an error listener.
  const req = mod.get({ host: host, port: 42 }, common.mustNotCall());
  req.on('error', common.mustCall(function(err) {
    if (err.code === 'EAGAIN' && tries < MAX_TRIES) {
      tryGet(mod, ++tries);
      return;
    }
    assert(errCodes.includes(err.code), err);
  }));
  // http.get() called req1.end() for us
}

function tryRequest(mod, tries) {
  const req = mod.request({
    method: 'GET',
    host: host,
    port: 42
  }, common.mustNotCall());
  req.on('error', common.mustCall(function(err) {
    if (err.code === 'EAGAIN' && tries < MAX_TRIES) {
      tryRequest(mod, ++tries);
      return;
    }
    assert(errCodes.includes(err.code), err);
  }));
  req.end();
}

function test(mod) {
  tryGet(mod, 0);
  tryRequest(mod, 0);
}

if (common.hasCrypto) {
  test(https);
} else {
  common.printSkipMessage('missing crypto');
}

test(http);
