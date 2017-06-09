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
const assert = require('assert');
const http = require('http');

const bufferSize = 5 * 1024 * 1024;
let measuredSize = 0;

const buffer = Buffer.allocUnsafe(bufferSize);
for (let i = 0; i < buffer.length; i++) {
  buffer[i] = i % 256;
}


const web = http.Server(function(req, res) {
  web.close();

  console.log(req.headers);

  let i = 0;

  req.on('data', function(d) {
    process.stdout.write(',');
    measuredSize += d.length;
    for (let j = 0; j < d.length; j++) {
      assert.strictEqual(buffer[i], d[j]);
      i++;
    }
  });


  req.on('end', function() {
    res.writeHead(200);
    res.write('thanks');
    res.end();
    console.log('response with \'thanks\'');
  });

  req.connection.on('error', function(e) {
    console.log('http server-side error: ' + e.message);
    process.exit(1);
  });
});

web.listen(0, common.mustCall(function() {
  console.log('Making request');

  const req = http.request({
    port: this.address().port,
    method: 'GET',
    path: '/',
    headers: { 'content-length': buffer.length }
  }, common.mustCall(function(res) {
    console.log('Got response');
    res.setEncoding('utf8');
    res.on('data', common.mustCall(function(string) {
      assert.strictEqual('thanks', string);
    }));
  }));
  req.end(buffer);
}));


process.on('exit', function() {
  assert.strictEqual(bufferSize, measuredSize);
});
