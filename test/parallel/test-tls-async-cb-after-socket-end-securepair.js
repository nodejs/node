'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');
const SSL_OP_NO_TICKET = require('crypto').constants.SSL_OP_NO_TICKET;
const assert = require('assert');
const net = require('net');
const tls = require('tls');

const options = {
  secureOptions: SSL_OP_NO_TICKET,
  key: fixtures.readSync('test_key.pem'),
  cert: fixtures.readSync('test_cert.pem')
};

const server = net.createServer(function(socket) {
  const sslcontext = tls.createSecureContext(options);
  sslcontext.context.setCiphers('RC4-SHA:AES128-SHA:AES256-SHA');

  const pair = tls.createSecurePair(sslcontext, true, false, false, { server });

  assert.ok(pair.encrypted.writable);
  assert.ok(pair.cleartext.writable);

  pair.encrypted.pipe(socket);
  socket.pipe(pair.encrypted);

  pair.on('error', () => {});  // Expected, client s1 closes connection.
});

let sessionCb = null;
let client = null;

server.on('newSession', common.mustCall(function(key, session, done) {
  done();
}));

server.on('resumeSession', common.mustCall(function(id, cb) {
  sessionCb = cb;

  next();
}));

server.listen(0, function() {
  const clientOpts = {
    port: this.address().port,
    rejectUnauthorized: false,
    session: false
  };

  const s1 = tls.connect(clientOpts, function() {
    clientOpts.session = s1.getSession();
    console.log('1st secure');

    s1.destroy();
    const s2 = tls.connect(clientOpts, function(s) {
      console.log('2nd secure');

      s2.destroy();
    }).on('connect', function() {
      console.log('2nd connected');
      client = s2;

      next();
    });
  });
});

function next() {
  if (!client || !sessionCb)
    return;

  client.destroy();
  setTimeout(function() {
    sessionCb();
    server.close();
  }, 100);
}
