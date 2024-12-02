'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(common.mustCall(function(req, res) {
  assert.deepStrictEqual(req.rawHeaders, [
    'host', `127.0.0.1:${server.address().port}`,
    'foo', 'bar',
    'test', 'value',
    'foo', 'baz',
  ]);

  res.end('ok');
  server.close();
}));
server.listen(0, '127.0.0.1', function() {
  http.request({
    method: 'POST',
    host: '127.0.0.1',
    port: this.address().port,
    setDefaultHeaders: false,
    headers: [
      'host', `127.0.0.1:${server.address().port}`,
      'foo', 'bar',
      'test', 'value',
      'foo', 'baz',
    ]
  }).end();
});
