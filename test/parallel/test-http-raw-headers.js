var common = require('../common');
var assert = require('assert');

var http = require('http');

http.createServer(function(req, res) {
  this.close();
  var expectRawHeaders = [
    'Host',
    'localhost:' + common.PORT,
    'transfer-ENCODING',
    'CHUNKED',
    'x-BaR',
    'yoyoyo',
    'Connection',
    'close'
  ];
  var expectHeaders = {
    host: 'localhost:' + common.PORT,
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

  assert.deepEqual(req.rawHeaders, expectRawHeaders);
  assert.deepEqual(req.headers, expectHeaders);

  req.on('end', function() {
    assert.deepEqual(req.rawTrailers, expectRawTrailers);
    assert.deepEqual(req.trailers, expectTrailers);
  });

  req.resume();
  res.addTrailers([
    ['x-fOo', 'xOxOxOx'],
    ['x-foO', 'OxOxOxO'],
    ['X-fOo', 'xOxOxOx'],
    ['X-foO', 'OxOxOxO']
  ]);
  res.end('x f o o');
}).listen(common.PORT, function() {
  var expectRawHeaders = [
    'Date',
    'Tue, 06 Aug 2013 01:31:54 GMT',
    'Connection',
    'close',
    'Transfer-Encoding',
    'chunked'
  ];
  var req = http.request({ port: common.PORT, path: '/' });
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
      'Date',
      null,
      'Connection',
      'close',
      'Transfer-Encoding',
      'chunked'
    ];
    var expectHeaders = {
      date: null,
      connection: 'close',
      'transfer-encoding': 'chunked'
    };
    res.rawHeaders[1] = null;
    res.headers.date = null;
    assert.deepEqual(res.rawHeaders, expectRawHeaders);
    assert.deepEqual(res.headers, expectHeaders);
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

      assert.deepEqual(res.rawTrailers, expectRawTrailers);
      assert.deepEqual(res.trailers, expectTrailers);
      console.log('ok');
    });
    res.resume();
  });
});
