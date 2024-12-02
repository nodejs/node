'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(common.mustCall(function(req, res) {
  assert.deepStrictEqual(req.rawHeaders, [
    'test', 'value',
    'HOST', `127.0.0.1:${server.address().port}`,
    'foo', 'bar',
    'foo', 'baz',
    'connection', 'close',
  ]);

  res.end('ok');
  server.close();
}));
server.listen(0, '127.0.0.1', function() {
  const req = http.request({
    method: 'POST',
    host: '127.0.0.1',
    port: this.address().port,
    setDefaultHeaders: false,
  });

  req.setHeader('test', 'value');
  req.setHeader('HOST', `127.0.0.1:${server.address().port}`);
  req.setHeader('foo', ['bar', 'baz']);
  req.setHeader('connection', 'close');

  req.end();
});
