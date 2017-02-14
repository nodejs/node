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

const options = {
  key: fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))
};

const server = tls.createServer(options, common.mustCall(function(socket) {
  socket.on('data', function(data) {
    console.error(data.toString());
    assert.strictEqual(data.toString(), 'ok');
  });
}, 3)).listen(0, function() {
  unauthorized();
});

function unauthorized() {
  const socket = tls.connect({
    port: server.address().port,
    servername: 'localhost',
    rejectUnauthorized: false
  }, common.mustCall(function() {
    assert(!socket.authorized);
    socket.end();
    rejectUnauthorized();
  }));
  socket.on('error', common.mustNotCall());
  socket.write('ok');
}

function rejectUnauthorized() {
  const socket = tls.connect(server.address().port, {
    servername: 'localhost'
  }, common.mustNotCall());
  socket.on('error', common.mustCall(function(err) {
    console.error(err);
    authorized();
  }));
  socket.write('ng');
}

function authorized() {
  const socket = tls.connect(server.address().port, {
    ca: [fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))],
    servername: 'localhost'
  }, common.mustCall(function() {
    assert(socket.authorized);
    socket.end();
    server.close();
  }));
  socket.on('error', common.mustNotCall());
  socket.write('ok');
}
