'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var tls = require('tls');

var fs = require('fs');
var net = require('net');


var received = '';
var ended = 0;

var server = tls.createServer({
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
}, function(c) {
  c._write('hello ', null, function() {
    c._write('world!', null, function() {
      c.destroy();
    });
    c._write(' gosh', null, function() {});
  });

  server.close();
}).listen(common.PORT, function() {
  var c = tls.connect(common.PORT, {
    rejectUnauthorized: false
  }, function() {
    c.on('data', function(chunk) {
      received += chunk;
    });
    c.on('end', function() {
      ended++;
    });
  });
});

process.on('exit', function() {
  assert.equal(ended, 1);
  assert.equal(received, 'hello world! gosh');
});
