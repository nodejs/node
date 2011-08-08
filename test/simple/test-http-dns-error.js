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
var https = require('https');

var expected_bad_requests = 0;
var actual_bad_requests = 0;

var host = '********';
host += host;
host += host;
host += host;
host += host;
host += host;

function do_not_call() {
  throw new Error("This function should not have been called.");
}

function test(mod) {
  expected_bad_requests += 2;

  // Bad host name should not throw an uncatchable exception.
  // Ensure that there is time to attach an error listener.
  var req = mod.get({host:host, port:42}, do_not_call);
  req.on('error', function(err) {
   assert.equal(err.code, 'EBADNAME');
   actual_bad_requests++;
  });
  // http.get() called req.end() for us

  var req = mod.request({method:'GET', host:host, port:42}, do_not_call);
  req.on('error', function(err) {
   assert.equal(err.code, 'EBADNAME');
   actual_bad_requests++;
  });
  req.end();
}

// FIXME This doesn't work for https because the tls module won't emit errors
// until a secure channel has been established - and that is never going to
// happen because the host name is invalid.
//test(https);
test(http);

process.on('exit', function() {
  assert.equal(actual_bad_requests, expected_bad_requests);
});