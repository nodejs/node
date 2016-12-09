'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const assert = require('assert');
const tls = require('tls');

const fs = require('fs');
const path = require('path');
const net = require('net');

const options = {
  key: fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))
};

const body = 'A'.repeat(40000);

// the "proxy" server
const a = tls.createServer(options, function(socket) {
  const myOptions = {
    host: '127.0.0.1',
    port: b.address().port,
    rejectUnauthorized: false
  };
  const dest = net.connect(myOptions);
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
    const myOptions = {
      host: '127.0.0.1',
      port: a.address().port,
      rejectUnauthorized: false
    };
    const socket = tls.connect(myOptions);
    const ssl = tls.connect({
      socket: socket,
      rejectUnauthorized: false
    });
    ssl.setEncoding('utf8');
    var buf = '';
    ssl.on('data', function(data) {
      buf += data;
    });
    ssl.on('end', common.mustCall(function() {
      assert.strictEqual(buf, body);
      ssl.end();
      a.close();
      b.close();
    }));
  });
});
