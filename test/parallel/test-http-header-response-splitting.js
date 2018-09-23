'use strict';
var common = require('../common'),
    assert = require('assert'),
    http = require('http');

var testIndex = 0;
const testCount = 2 * 4 * 6;
const responseBody = 'Hi mars!';

var server = http.createServer(function(req, res) {
  function reply(header) {
    switch (testIndex % 4) {
    case 0:
      res.writeHead(200, { a: header, b: header });
      break;
    case 1:
      res.setHeader('a', header);
      res.setHeader('b', header);
      res.writeHead(200);
      break;
    case 2:
      res.setHeader('a', header);
      res.writeHead(200, { b: header });
      break;
    case 3:
      res.setHeader('a', [header]);
      res.writeHead(200, { b: header });
      break;
    default:
      assert.fail(null, null, 'unreachable');
    }
    res.write(responseBody);
    if (testIndex % 8 < 4) {
      res.addTrailers({ ta: header, tb: header });
    } else {
      res.addTrailers([['ta', header], ['tb', header]]);
    }
    res.end();
  }
  switch ((testIndex / 8) | 0) {
    case 0:
      reply('foo \r\ninvalid: bar');
      break;
    case 1:
      reply('foo \ninvalid: bar');
      break;
    case 2:
      reply('foo \rinvalid: bar');
      break;
    case 3:
      reply('foo \n\n\ninvalid: bar');
      break;
    case 4:
      reply('foo \r\n \r\n \r\ninvalid: bar');
      break;
    case 5:
      reply('foo \r \n \r \n \r \ninvalid: bar');
      break;
    default:
      assert(false);
  }
  if (++testIndex === testCount) {
    server.close();
  }
});

server.listen(common.PORT, common.mustCall(function() {
  for (var i = 0; i < testCount; i++) {
    http.get({ port: common.PORT, path: '/' }, common.mustCall(function(res) {
      assert.strictEqual(res.headers.a, 'foo invalid: bar');
      assert.strictEqual(res.headers.b, 'foo invalid: bar');
      assert.strictEqual(res.headers.foo, undefined);
      assert.strictEqual(res.headers.invalid, undefined);
      var data = '';
      res.setEncoding('utf8');
      res.on('data', function(s) { data += s; });
      res.on('end', common.mustCall(function() {
        assert.equal(data, responseBody);
        assert.strictEqual(res.trailers.ta, 'foo invalid: bar');
        assert.strictEqual(res.trailers.tb, 'foo invalid: bar');
        assert.strictEqual(res.trailers.foo, undefined);
        assert.strictEqual(res.trailers.invalid, undefined);
      }));
      res.resume();
    }));
  }
}));
