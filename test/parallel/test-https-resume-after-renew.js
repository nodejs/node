'use strict';
var common = require('../common');
var fs = require('fs');
var https = require('https');
var crypto = require('crypto');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem'),
  ca: fs.readFileSync(common.fixturesDir + '/keys/ca1-cert.pem')
};

var server = https.createServer(options, function(req, res) {
  res.end('hello');
});

var aes = Buffer.alloc(16, 'S');
var hmac = Buffer.alloc(16, 'H');

server._sharedCreds.context.enableTicketKeyCallback();
server._sharedCreds.context.onticketkeycallback = function(name, iv, enc) {
  if (enc) {
    var newName = Buffer.alloc(16, 'A');
    var newIV = crypto.randomBytes(16);
  } else {
    // Renew
    return [ 2, hmac, aes ];
  }

  return [ 1, hmac, aes, newName, newIV ];
};

server.listen(0, function() {
  var addr = this.address();

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
