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

var fs = require('fs');
var dgram = require('dgram');
var callbacks = 0;
var client;
var timer;

if (process.platform === 'darwin') {
  console.error('Test is disabled due to 17894467 Apple bug');
  return;
}

client = dgram.createSocket('udp4');

client.bind(common.PORT);

function callback() {
  callbacks++;
  if (callbacks == 2) {
    clearTimeout(timer);
    client.close();
  } else if (callbacks > 2) {
    throw new Error("the callbacks should be called only two times");
  }
}

client.on('message', function (buffer, bytes) {
  callback();
});

client.send(new Buffer(1), 0, 0, common.PORT, "127.0.0.1", function (err, len) {
  callback();
});

timer = setTimeout(function() {
  throw new Error('Timeout');
}, 200);
