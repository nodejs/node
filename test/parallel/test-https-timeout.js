'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var https = require('https');

var fs = require('fs');
var exec = require('child_process').exec;

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

// a server that never replies
var server = https.createServer(options, function() {
  console.log('Got request.  Doing nothing.');
}).listen(common.PORT, function() {
  var req = https.request({
    host: 'localhost',
    port: common.PORT,
    path: '/',
    method: 'GET',
    rejectUnauthorized: false
  });
  req.setTimeout(10);
  req.end();

  req.on('response', function(res) {
    console.log('got response');
  });

  req.on('socket', function() {
    console.log('got a socket');

    req.socket.on('connect', function() {
      console.log('socket connected');
    });

    setTimeout(function() {
      throw new Error('Did not get timeout event');
    }, 200);
  });

  req.on('timeout', function() {
    console.log('timeout occurred outside');
    req.destroy();
    server.close();
    process.exit(0);
  });
});
