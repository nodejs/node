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
  maxVersion: 'TLSv1.2',
  secureOptions: SSL_OP_NO_TICKET,
  key: fixtures.readSync('test_key.pem'),
  cert: fixtures.readSync('test_cert.pem')
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
