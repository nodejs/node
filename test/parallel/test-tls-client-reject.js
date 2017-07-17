'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readSync('test_key.pem'),
  cert: fixtures.readSync('test_cert.pem')
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
    ca: [fixtures.readSync('test_cert.pem')],
    servername: 'localhost'
  }, common.mustCall(function() {
    assert(socket.authorized);
    socket.end();
    server.close();
  }));
  socket.on('error', common.mustNotCall());
  socket.write('ok');
}
