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

http.createServer(function(req, res) {
  const expectRawHeaders = [
    'Host',
    `localhost:${this.address().port}`,
    'transfer-ENCODING',
    'CHUNKED',
    'x-BaR',
    'yoyoyo',
    'Connection',
    'keep-alive',
  ];
  const expectHeaders = {
    'host': `localhost:${this.address().port}`,
    'transfer-encoding': 'CHUNKED',
    'x-bar': 'yoyoyo',
    'connection': 'keep-alive'
  };
  const expectRawTrailers = [
    'x-bAr',
    'yOyOyOy',
    'x-baR',
    'OyOyOyO',
    'X-bAr',
    'yOyOyOy',
    'X-baR',
    'OyOyOyO',
  ];
  const expectTrailers = { 'x-bar': 'yOyOyOy, OyOyOyO, yOyOyOy, OyOyOyO' };

  this.close();

  assert.deepStrictEqual(req.rawHeaders, expectRawHeaders);
  assert.deepStrictEqual(req.headers, expectHeaders);

  req.on('end', function() {
    assert.deepStrictEqual(req.rawTrailers, expectRawTrailers);
    assert.deepStrictEqual(req.trailers, expectTrailers);
  });

  req.resume();
  res.setHeader('Keep-Alive', 'timeout=1');
  res.setHeader('Trailer', 'x-foo');
  res.addTrailers([
    ['x-fOo', 'xOxOxOx'],
    ['x-foO', 'OxOxOxO'],
    ['X-fOo', 'xOxOxOx'],
    ['X-foO', 'OxOxOxO'],
  ]);
  res.end('x f o o');
}).listen(0, function() {
  const req = http.request({ port: this.address().port, path: '/' });
  req.addTrailers([
    ['x-bAr', 'yOyOyOy'],
    ['x-baR', 'OyOyOyO'],
    ['X-bAr', 'yOyOyOy'],
    ['X-baR', 'OyOyOyO'],
  ]);
  req.setHeader('transfer-ENCODING', 'CHUNKED');
  req.setHeader('x-BaR', 'yoyoyo');
  req.end('y b a r');
  req.on('response', function(res) {
    const expectRawHeaders = [
      'Keep-Alive',
      'timeout=1',
      'Trailer',
      'x-foo',
      'Date',
      null,
      'Connection',
      'keep-alive',
      'Transfer-Encoding',
      'chunked',
    ];
    const expectHeaders = {
      'keep-alive': 'timeout=1',
      'trailer': 'x-foo',
      'date': null,
      'connection': 'keep-alive',
      'transfer-encoding': 'chunked'
    };
    res.rawHeaders[5] = null;
    res.headers.date = null;
    assert.deepStrictEqual(res.rawHeaders, expectRawHeaders);
    assert.deepStrictEqual(res.headers, expectHeaders);
    res.on('end', function() {
      const expectRawTrailers = [
        'x-fOo',
        'xOxOxOx',
        'x-foO',
        'OxOxOxO',
        'X-fOo',
        'xOxOxOx',
        'X-foO',
        'OxOxOxO',
      ];
      const expectTrailers = { 'x-foo': 'xOxOxOx, OxOxOxO, xOxOxOx, OxOxOxO' };

      assert.deepStrictEqual(res.rawTrailers, expectRawTrailers);
      assert.deepStrictEqual(res.trailers, expectTrailers);
      console.log('ok');
    });
    res.resume();
  });
});
