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

var common = require('../common');
var assert = require('assert');
var http = require('http');

// sending `agent: false` when `port: null` is also passed in (i.e. the result of
// a `url.parse()` call with the default port used, 80 or 443), should not result
// in an assertion error...
var opts = {
  host: '127.0.0.1',
  port: null,
  path: '/',
  method: 'GET',
  agent: false
};

var good = false;
process.on('exit', function() {
  assert(good, 'expected either an "error" or "response" event');
});

// we just want an "error" (no local HTTP server on port 80) or "response"
// to happen (user happens ot have HTTP server running on port 80). As long as the
// process doesn't crash from a C++ assertion then we're good.
var req = http.request(opts);
req.on('response', function(res) {
  good = true;
});
req.on('error', function(err) {
  // an "error" event is ok, don't crash the process
  good = true;
});
req.end();
