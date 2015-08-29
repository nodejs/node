'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');
const net = require('net');

const sent = 'hello world';

const serverOptions = {
  isServer: true,
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

function testSocketOptions(socket, socketOptions) {
  let received = '';
  const server = tls.createServer(serverOptions, function(s) {
    s.on('data', function(chunk) {
      received += chunk;
    });

    s.on('end', function() {
      server.close();
      s.destroy();
      assert.equal(received, sent);
      setImmediate(runTests);
    });
  }).listen(common.PORT, function() {
    let c = new tls.TLSSocket(socket, socketOptions);
    c.connect(common.PORT, function() {
      c.end(sent);
    });
  });

}

const testArgs = [
  [],
  [undefined, {}]
];

let n = 0;
function runTests() {
  if (n++ < testArgs.length) {
    testSocketOptions.apply(null, testArgs[n]);
  }
}

runTests();
