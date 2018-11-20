// Server sends a large string. Client counts bytes and pauses every few
// seconds. Makes sure that pause and resume work properly.

var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var tls = require('tls');
var fs = require('fs');


var body = '';

process.stdout.write('build body...');
for (var i = 0; i < 1024 * 1024; i++) {
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
  var client = tls.connect({
    port: common.PORT,
    rejectUnauthorized: false
  });

  client.on('data', function(d) {
    process.stdout.write('.');
    recvCount += d.length;

    client.pause();
    process.nextTick(function() {
      client.resume();
    });
  });


  client.on('close', function() {
    console.error('close');
    server.close();
    clearTimeout(timeout);
  });
});


function displayCounts() {
  console.log('body.length: %d', body.length);
  console.log('  recvCount: %d', recvCount);
}


var timeout = setTimeout(displayCounts, 10 * 1000);


process.on('exit', function() {
  displayCounts();
  assert.equal(1, connections);
  assert.equal(body.length, recvCount);
});
