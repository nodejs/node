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

const server = tls.Server(options, function(socket) {
  setImmediate(function() {
    console.log('sending');
    verify();
    setImmediate(function() {
      socket.destroy();
    });
  });
});

function verify() {
  console.log('verify');
  crypto.createVerify('RSA-SHA1')
    .update('Test')
    .verify(certPem, 'asdfasdfas', 'base64');
}

server.listen(0, function() {
  tls.connect({
    port: this.address().port,
    rejectUnauthorized: false
  }, function() {
    verify();
  }).on('data', function(data) {
    console.log(data);
  }).on('error', function(err) {
    throw err;
  }).on('close', function() {
    server.close();
  }).resume();
});

server.unref();
