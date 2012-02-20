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
var https = require('https');
var tls = require('tls');

var options = {
  host: 'github.com',
  path: '/kriskowal/tigerblood/',
  port: 443
};

var req = https.get(options, function(response) {
  var recved = 0;

  response.on('data', function(chunk) {
    recved += chunk.length;
    console.log('Response data.');
  });

  response.on('end', function() {
    console.log('Response end.');
    // Does not work
    loadDom();
  });

});

req.on('error', function(e) {
  console.log('Error on get.');
});

function loadDom() {
  // Do a lot of computation to stall the process.
  // In the meantime the socket will be disconnected.
  for (var i = 0; i < 1e8; i++);

  console.log('Dom loaded.');
}
