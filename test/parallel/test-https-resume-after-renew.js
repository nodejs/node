'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const fs = require('fs');
const https = require('https');
const crypto = require('crypto');

const options = {
  key: fs.readFileSync(`${common.fixturesDir}/keys/agent1-key.pem`),
  cert: fs.readFileSync(`${common.fixturesDir}/keys/agent1-cert.pem`),
  ca: fs.readFileSync(`${common.fixturesDir}/keys/ca1-cert.pem`)
};

const server = https.createServer(options, function(req, res) {
  res.end('hello');
});

const aes = Buffer.alloc(16, 'S');
const hmac = Buffer.alloc(16, 'H');

server._sharedCreds.context.enableTicketKeyCallback();
server._sharedCreds.context.onticketkeycallback = function(name, iv, enc) {
  let newName, newIV;
  if (enc) {
    newName = Buffer.alloc(16, 'A');
    newIV = crypto.randomBytes(16);
  } else {
    // Renew
    return [ 2, hmac, aes ];
  }

  return [ 1, hmac, aes, newName, newIV ];
};

server.listen(0, function() {
  const addr = this.address();

  function doReq(callback) {
    https.request({
      method: 'GET',
      port: addr.port,
      servername: 'agent1',
      ca: options.ca
    }, function(res) {
      res.resume();
      res.once('end', callback);
    }).end();
  }

  doReq(function() {
    doReq(function() {
      server.close();
    });
  });
});
