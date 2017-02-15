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
    'close'
  ];
  const expectHeaders = {
    host: `localhost:${this.address().port}`,
    'transfer-encoding': 'CHUNKED',
    'x-bar': 'yoyoyo',
    connection: 'close'
  };
  const expectRawTrailers = [
    'x-bAr',
    'yOyOyOy',
    'x-baR',
    'OyOyOyO',
    'X-bAr',
    'yOyOyOy',
    'X-baR',
    'OyOyOyO'
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
  res.setHeader('Trailer', 'x-foo');
  res.addTrailers([
    ['x-fOo', 'xOxOxOx'],
    ['x-foO', 'OxOxOxO'],
    ['X-fOo', 'xOxOxOx'],
    ['X-foO', 'OxOxOxO']
  ]);
  res.end('x f o o');
}).listen(0, function() {
  const req = http.request({ port: this.address().port, path: '/' });
  req.addTrailers([
    ['x-bAr', 'yOyOyOy'],
    ['x-baR', 'OyOyOyO'],
    ['X-bAr', 'yOyOyOy'],
    ['X-baR', 'OyOyOyO']
  ]);
  req.setHeader('transfer-ENCODING', 'CHUNKED');
  req.setHeader('x-BaR', 'yoyoyo');
  req.end('y b a r');
  req.on('response', function(res) {
    const expectRawHeaders = [
      'Trailer',
      'x-foo',
      'Date',
      null,
      'Connection',
      'close',
      'Transfer-Encoding',
      'chunked'
    ];
    const expectHeaders = {
      trailer: 'x-foo',
      date: null,
      connection: 'close',
      'transfer-encoding': 'chunked'
    };
    res.rawHeaders[3] = null;
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
        'OxOxOxO'
      ];
      const expectTrailers = { 'x-foo': 'xOxOxOx, OxOxOxO, xOxOxOx, OxOxOxO' };

      assert.deepStrictEqual(res.rawTrailers, expectRawTrailers);
      assert.deepStrictEqual(res.trailers, expectTrailers);
      console.log('ok');
    });
    res.resume();
  });
});
