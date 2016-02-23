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

var aes = new Buffer(16);
aes.fill('S');
var hmac = new Buffer(16);
hmac.fill('H');

server._sharedCreds.context.enableTicketKeyCallback();
server._sharedCreds.context.onticketkeycallback = function(name, iv, enc) {
  if (enc) {
    var newName = new Buffer(16);
    var newIV = crypto.randomBytes(16);
    newName.fill('A');
  } else {
    // Renew
    return [ 2, hmac, aes ];
  }

  return [ 1, hmac, aes, newName, newIV ];
};

server.listen(common.PORT, function() {
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
