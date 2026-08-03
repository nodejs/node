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
require('../common');
const assert = require('assert');
const http = require('http');

const expectedServer = 'Request Body from Client';
let resultServer = '';
const expectedClient = 'Response Body from Server';
let resultClient = '';

const server = http.createServer((req, res) => {
  console.error('pause server request');
  req.pause();
  setTimeout(() => {
    console.error('resume server request');
    req.resume();
    req.setEncoding('utf8');
    req.on('data', (chunk) => {
      resultServer += chunk;
    });
    req.on('end', () => {
      console.error(resultServer);
      res.writeHead(200);
      res.end(expectedClient);
    });
  }, 100);
});

server.listen(0, function() {
  // Anonymous function rather than arrow function to test `this` value.
  assert.strictEqual(this, server);
  const req = http.request({
    port: this.address().port,
    path: '/',
    method: 'POST'
  }, (res) => {
    console.error('pause client response');
    res.pause();
    setTimeout(() => {
      console.error('resume client response');
      res.resume();
      res.on('data', (chunk) => {
        resultClient += chunk;
      });
      res.on('end', () => {
        console.error(resultClient);
        server.close();
      });
    }, 100);
  });
  req.end(expectedServer);
});

process.on('exit', () => {
  assert.strictEqual(resultServer, expectedServer);
  assert.strictEqual(resultClient, expectedClient);
});
