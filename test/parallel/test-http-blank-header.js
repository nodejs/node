'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const server = http.createServer(common.mustCall(function(req, res) {
  assert.equal('GET', req.method);
  assert.equal('/blah', req.url);
  assert.deepStrictEqual({
    host: 'mapdevel.trolologames.ru:443',
    origin: 'http://mapdevel.trolologames.ru',
    cookie: ''
  }, req.headers);
}));


server.listen(0, function() {
  const c = net.createConnection(this.address().port);

  c.on('connect', function() {
    c.write('GET /blah HTTP/1.1\r\n' +
            'Host: mapdevel.trolologames.ru:443\r\n' +
            'Cookie:\r\n' +
            'Origin: http://mapdevel.trolologames.ru\r\n' +
            '\r\n\r\nhello world'
    );
  });

  c.on('end', function() {
    c.end();
  });

  c.on('close', function() {
    server.close();
  });
});
