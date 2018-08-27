'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');
const SSL_OP_NO_TICKET = require('crypto').constants.SSL_OP_NO_TICKET;
const tls = require('tls');

// Check tls async callback after socket ends

const options = {
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
