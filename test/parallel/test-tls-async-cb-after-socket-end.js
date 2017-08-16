'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const path = require('path');
const fs = require('fs');
const SSL_OP_NO_TICKET = require('crypto').constants.SSL_OP_NO_TICKET;

const tls = require('tls');

const options = {
  secureOptions: SSL_OP_NO_TICKET,
  key: fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))
};

const server = tls.createServer(options, function(c) {
});

let sessionCb = null;
let client = null;

server.on('newSession', function(key, session, done) {
  done();
});

server.on('resumeSession', function(id, cb) {
  sessionCb = cb;

  next();
});

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
