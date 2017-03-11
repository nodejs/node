'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');
const path = require('path');
const net = require('net');

let options;

const body = new Buffer(400000).fill('A');

options = {
  key: fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))
};

// the "proxy" server
const a = tls.createServer(options, function(socket) {
  const options = {
    host: '127.0.0.1',
    port: b.address().port,
    rejectUnauthorized: false
  };
  const dest = net.connect(options);
  dest.pipe(socket);
  socket.pipe(dest);

  dest.on('end', function() {
    socket.destroy();
  });
});

// the "target" server
const b = tls.createServer(options, function(socket) {
  socket.end(body);
});

a.listen(0, function() {
  b.listen(0, function() {
    options = {
      host: '127.0.0.1',
      port: a.address().port,
      rejectUnauthorized: false
    };
    const socket = tls.connect(options);
    const ssl = tls.connect({
      socket: socket,
      rejectUnauthorized: false
    });
    ssl.setEncoding('utf8');
    let buf = '';
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
