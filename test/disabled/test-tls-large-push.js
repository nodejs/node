/* eslint-disable no-debugger */
'use strict';
// Server sends a large string. Client counts bytes and pauses every few
// seconds. Makes sure that pause and resume work properly.
var common = require('../common');
var assert = require('assert');
var tls = require('tls');
var fs = require('fs');


var body = '';

process.stdout.write('build body...');
for (var i = 0; i < 10 * 1024 * 1024; i++) {
  body += 'hello world\n';
}
process.stdout.write('done\n');


var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent2-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent2-cert.pem')
};

var connections = 0;


var server = tls.Server(options, function(socket) {
  socket.end(body);
  connections++;
});

var recvCount = 0;

server.listen(common.PORT, function() {
  var client = tls.connect(common.PORT);

  client.on('data', function(d) {
    process.stdout.write('.');
    recvCount += d.length;

    /*
    client.pause();
    process.nextTick(function() {
      client.resume();
    });
    */
  });


  client.on('close', function() {
    debugger;
    console.log('close');
    server.close();
  });
});



process.on('exit', function() {
  assert.equal(1, connections);
  console.log('body.length: %d', body.length);
  console.log('  recvCount: %d', recvCount);
  assert.equal(body.length, recvCount);
});
