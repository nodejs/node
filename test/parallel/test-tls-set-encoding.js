'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var tls = require('tls');

var fs = require('fs');


var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent2-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent2-cert.pem')
};

var connections = 0;
var message = 'hello world\n';


var server = tls.Server(options, function(socket) {
  socket.end(message);
  connections++;
});


server.listen(common.PORT, function() {
  var client = tls.connect({
    port: common.PORT,
    rejectUnauthorized: false
  });

  var buffer = '';

  client.setEncoding('utf8');

  client.on('data', function(d) {
    assert.ok(typeof d === 'string');
    buffer += d;
  });


  client.on('close', function() {
    // readyState is deprecated but we want to make
    // sure this isn't triggering an assert in lib/net.js
    // See issue #1069.
    assert.equal('closed', client.readyState);

    assert.equal(buffer, message);
    console.log(message);
    server.close();
  });
});


process.on('exit', function() {
  assert.equal(1, connections);
});
