'use strict';
require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const s = http.createServer(function(req, res) {
  res.statusCode = 200;
  res.statusMessage = 'Custom Message';
  res.end('');
});

s.listen(0, test);


function test() {
  const bufs = [];
  const client = net.connect(this.address().port, function() {
    client.write('GET / HTTP/1.1\r\nConnection: close\r\n\r\n');
  });
  client.on('data', function(chunk) {
    bufs.push(chunk);
  });
  client.on('end', function() {
    const head = Buffer.concat(bufs).toString('latin1').split('\r\n')[0];
    assert.equal('HTTP/1.1 200 Custom Message', head);
    console.log('ok');
    s.close();
  });
}
