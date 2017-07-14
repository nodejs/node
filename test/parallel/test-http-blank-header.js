'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const server = http.createServer(common.mustCall((req, res) => {
  assert.strictEqual('GET', req.method);
  assert.strictEqual('/blah', req.url);
  assert.deepStrictEqual({
    host: 'example.org:443',
    origin: 'http://example.org',
    cookie: ''
  }, req.headers);
}));


server.listen(0, common.mustCall(() => {
  const c = net.createConnection(server.address().port);

  c.on('connect', common.mustCall(() => {
    c.write('GET /blah HTTP/1.1\r\n' +
            'Host: example.org:443\r\n' +
            'Cookie:\r\n' +
            'Origin: http://example.org\r\n' +
            '\r\n\r\nhello world'
    );
  }));

  c.on('end', common.mustCall(() => c.end()));
  c.on('close', common.mustCall(() => server.close()));
}));
