'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');
var path = require('path');
var net = require('net');

var options, a, b;

var body = Buffer.alloc(400000, 'A');

options = {
  key: fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))
};

// the "proxy" server
a = tls.createServer(options, function(socket) {
  var options = {
    host: '127.0.0.1',
    port: b.address().port,
    rejectUnauthorized: false
  };
  var dest = net.connect(options);
  dest.pipe(socket);
  socket.pipe(dest);

  dest.on('end', function() {
    socket.destroy();
  });
});

// the "target" server
b = tls.createServer(options, function(socket) {
  socket.end(body);
});

a.listen(0, function() {
  b.listen(0, function() {
    options = {
      host: '127.0.0.1',
      port: a.address().port,
      rejectUnauthorized: false
    };
    var socket = tls.connect(options);
    var ssl;
    ssl = tls.connect({
      socket: socket,
      rejectUnauthorized: false
    });
    ssl.setEncoding('utf8');
    var buf = '';
    ssl.on('data', function(data) {
      buf += data;
    });
    ssl.on('end', common.mustCall(function() {
      assert.equal(buf, body);
      ssl.end();
      a.close();
      b.close();
    }));
  });
});
