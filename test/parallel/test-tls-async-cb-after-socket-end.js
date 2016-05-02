'use strict';

var common = require('../common');

var path = require('path');
var fs = require('fs');
const SSL_OP_NO_TICKET = require('crypto').constants.SSL_OP_NO_TICKET;

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

var tls = require('tls');

var options = {
  secureOptions: SSL_OP_NO_TICKET,
  key: fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))
};

var server = tls.createServer(options, function(c) {
});

var sessionCb = null;
var client = null;

server.on('newSession', function(key, session, done) {
  done();
});

server.on('resumeSession', function(id, cb) {
  sessionCb = cb;

  next();
});

server.listen(0, function() {
  var clientOpts = {
    port: this.address().port,
    rejectUnauthorized: false,
    session: false
  };

  var s1 = tls.connect(clientOpts, function() {
    clientOpts.session = s1.getSession();
    console.log('1st secure');

    s1.destroy();
    var s2 = tls.connect(clientOpts, function(s) {
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
