'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const crypto = require('crypto');
const tls = require('tls');
const fixtures = require('../common/fixtures');

crypto.DEFAULT_ENCODING = 'buffer';

const certPem = fixtures.readSync('test_cert.pem', 'ascii');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
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
  crypto.createVerify('SHA1')
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
