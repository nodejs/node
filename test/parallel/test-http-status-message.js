'use strict';
require('../common');
var assert = require('assert');
var http = require('http');
var net = require('net');

var s = http.createServer(function(req, res) {
  res.statusCode = 200;
  res.statusMessage = 'Custom Message';
  res.end('');
});

s.listen(0, test);


function test() {
  var bufs = [];
  var client = net.connect(this.address().port, function() {
    client.write('GET / HTTP/1.1\r\nConnection: close\r\n\r\n');
  });
  client.on('data', function(chunk) {
    bufs.push(chunk);
  });
  client.on('end', function() {
    var head = Buffer.concat(bufs).toString('latin1').split('\r\n')[0];
    assert.equal('HTTP/1.1 200 Custom Message', head);
    console.log('ok');
    s.close();
  });
}
