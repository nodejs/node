'use strict';
var common = require('../common'),
    assert = require('assert'),
    http = require('http');

var testIndex = 0,
    responses = 0;

var server = http.createServer(function(req, res) {
  switch (testIndex++) {
    case 0:
      res.writeHead(200, { test: 'foo \r\ninvalid: bar' });
      break;
    case 1:
      res.writeHead(200, { test: 'foo \ninvalid: bar' });
      break;
    case 2:
      res.writeHead(200, { test: 'foo \rinvalid: bar' });
      break;
    case 3:
      res.writeHead(200, { test: 'foo \n\n\ninvalid: bar' });
      break;
    case 4:
      res.writeHead(200, { test: 'foo \r\n \r\n \r\ninvalid: bar' });
      server.close();
      break;
    default:
      assert(false);
  }
  res.end('Hi mars!');
});

server.listen(common.PORT, function() {
  for (var i = 0; i < 5; i++) {
    var req = http.get({ port: common.PORT, path: '/' }, function(res) {
      assert.strictEqual(res.headers.test, 'foo invalid: bar');
      assert.strictEqual(res.headers.invalid, undefined);
      responses++;
      res.resume();
    });
  }
});

process.on('exit', function() {
  assert.strictEqual(responses, 5);
});
