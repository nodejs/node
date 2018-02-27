'use strict';
// Server sends a large string. Client counts bytes and pauses every few
// seconds. Makes sure that pause and resume work properly.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fs = require('fs');

process.stdout.write('build body...');
const body = 'hello world\n'.repeat(1024 * 1024);
process.stdout.write('done\n');

const options = {
  key: fs.readFileSync(`${common.fixturesDir}/keys/agent2-key.pem`),
  cert: fs.readFileSync(`${common.fixturesDir}/keys/agent2-cert.pem`)
};

const server = tls.Server(options, common.mustCall(function(socket) {
  socket.end(body);
}));

let recvCount = 0;

server.listen(common.PORT, function() {
  const client = tls.connect({
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
  console.log(`body.length: ${body.length}`);
  console.log(`  recvCount: ${recvCount}`);
}


let timeout = setTimeout(displayCounts, 10 * 1000);


process.on('exit', function() {
  displayCounts();
  assert.strictEqual(body.length, recvCount);
});
