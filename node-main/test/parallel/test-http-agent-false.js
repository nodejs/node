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
const http = require('http');

// Sending `agent: false` when `port: null` is also passed in (i.e. the result
// of a `url.parse()` call with the default port used, 80 or 443), should not
// result in an assertion error...
const opts = {
  host: '127.0.0.1',
  port: null,
  path: '/',
  method: 'GET',
  agent: false
};

// We just want an "error" (no local HTTP server on port 80) or "response"
// to happen (user happens ot have HTTP server running on port 80).
// As long as the process doesn't crash from a C++ assertion then we're good.
const req = http.request(opts);

// Will be called by either the response event or error event, not both
const oneResponse = common.mustCall();
req.on('response', oneResponse);
req.on('error', oneResponse);
req.end();
