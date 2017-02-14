'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const crypto = require('crypto');
const tls = require('tls');

crypto.DEFAULT_ENCODING = 'buffer';

const fs = require('fs');

const certPem = fs.readFileSync(common.fixturesDir + '/test_cert.pem', 'ascii');

const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

const server = tls.Server(options, (socket) => {
  setImmediate(() => {
    verify();
    setImmediate(() => {
      socket.destroy();
    });
  });
});

function verify() {
  crypto.createVerify('RSA-SHA1')
    .update('Test')
    .verify(certPem, 'asdfasdfas', 'base64');
}

server.listen(0, common.mustCall(() => {
  tls.connect({
    port: server.address().port,
    rejectUnauthorized: false
  }, common.mustCall(() => {
    verify();
  }))
    .on('error', common.mustNotCall())
    .on('close', common.mustCall(() => {
      server.close();
    })).resume();
}));

server.unref();
