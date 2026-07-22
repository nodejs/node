'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');
const SSL_OP_NO_TICKET = require('crypto').constants.SSL_OP_NO_TICKET;
const tls = require('tls');

// Check that TLS1.2 session resumption callbacks don't explode when made after
// the tls socket is destroyed. Disable TLS ticket support to force the legacy
// session resumption mechanism to be used.

// TLS1.2 is the last protocol version to support TLS sessions, after that the
// new and resume session events will never be emitted on the server.

const options = {
  secureOptions: SSL_OP_NO_TICKET,
  key: fixtures.readKey('rsa_private.pem'),
  cert: fixtures.readKey('rsa_cert.crt')
};

const server = tls.createServer(options, common.mustCall());

let sessionCb = null;
let client = null;

server.on('newSession', common.mustCall((key, session, done) => {
  done();
}));

server.on('resumeSession', common.mustCall((id, cb) => {
  sessionCb = cb;
  // Destroy the client and then call the session cb, to check that the cb
  // doesn't explode when called after the handle has been destroyed.
  next();
}));

server.listen(0, common.mustCall(() => {
  const clientOpts = {
    // Don't send a TLS1.3/1.2 ClientHello, they contain a fake session_id,
    // which triggers a 'resumeSession' event for client1. TLS1.2 ClientHello
    // won't have a session_id until client2, which will have a valid session.
    maxVersion: 'TLSv1.2',
    port: server.address().port,
    rejectUnauthorized: false,
    session: false
  };

  const s1 = tls.connect(clientOpts, common.mustCall(() => {
    clientOpts.session = s1.getSession();
    console.log('1st secure');

    s1.destroy();
    const s2 = tls.connect(clientOpts, (s) => {
      console.log('2nd secure');

      s2.destroy();
    }).on('connect', common.mustCall(() => {
      console.log('2nd connected');
      client = s2;

      next();
    }));
  }));
}));

function next() {
  if (!client || !sessionCb)
    return;

  client.destroy();
  setTimeout(common.mustCall(() => {
    sessionCb();
    server.close();
  }), 100);
}
