'use strict';
require('../common');
var assert = require('assert');

var http = require('http');

http.createServer(function(req, res) {
  var expectRawHeaders = [
    'Host',
    `localhost:${this.address().port}`,
    'transfer-ENCODING',
    'CHUNKED',
    'x-BaR',
    'yoyoyo',
    'Connection',
    'close'
  ];
  var expectHeaders = {
    host: `localhost:${this.address().port}`,
    'transfer-encoding': 'CHUNKED',
    'x-bar': 'yoyoyo',
    connection: 'close'
  };
  var expectRawTrailers = [
    'x-bAr',
    'yOyOyOy',
    'x-baR',
    'OyOyOyO',
    'X-bAr',
    'yOyOyOy',
    'X-baR',
    'OyOyOyO'
  ];
  var expectTrailers = { 'x-bar': 'yOyOyOy, OyOyOyO, yOyOyOy, OyOyOyO' };

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
  var req = http.request({ port: this.address().port, path: '/' });
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
    var expectRawHeaders = [
      'Trailer',
      'x-foo',
      'Date',
      null,
      'Connection',
      'close',
      'Transfer-Encoding',
      'chunked'
    ];
    var expectHeaders = {
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
      var expectRawTrailers = [
        'x-fOo',
        'xOxOxOx',
        'x-foO',
        'OxOxOxO',
        'X-fOo',
        'xOxOxOx',
        'X-foO',
        'OxOxOxO'
      ];
      var expectTrailers = { 'x-foo': 'xOxOxOx, OxOxOxO, xOxOxOx, OxOxOxO' };

      assert.deepStrictEqual(res.rawTrailers, expectRawTrailers);
      assert.deepStrictEqual(res.trailers, expectTrailers);
      console.log('ok');
    });
    res.resume();
  });
});
