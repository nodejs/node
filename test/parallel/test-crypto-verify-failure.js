var common = require('../common');
var assert = require('assert');

try {
  var crypto = require('crypto');
  var tls = require('tls');
} catch (e) {
  console.log('Not compiled with OPENSSL support.');
  process.exit();
}

crypto.DEFAULT_ENCODING = 'buffer';

var fs = require('fs');

var certPem = fs.readFileSync(common.fixturesDir + '/test_cert.pem', 'ascii');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var canSend = true;

var server = tls.Server(options, function(socket) {
  setImmediate(function() {
      console.log('sending');
      verify();
      setImmediate(function() {
        socket.destroy();
      });
  });
});

var client;

function verify() {
  console.log('verify');
  var verified = crypto.createVerify('RSA-SHA1')
                     .update('Test')
                     .verify(certPem, 'asdfasdfas', 'base64');
}

server.listen(common.PORT, function() {
  client = tls.connect({
    port: common.PORT,
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
