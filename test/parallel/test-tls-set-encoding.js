'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');


const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent2-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent2-cert.pem')
};

// Contains a UTF8 only character
const messageUtf8 = 'x√ab c';

// The same string above represented with ASCII
const messageAscii = 'xb\b\u001aab c';

const server = tls.Server(options, common.mustCall(function(socket) {
  socket.end(messageUtf8);
}));


server.listen(0, function() {
  const client = tls.connect({
    port: this.address().port,
    rejectUnauthorized: false
  });

  let buffer = '';

  client.setEncoding('ascii');

  client.on('data', function(d) {
    assert.ok(typeof d === 'string');
    buffer += d;
  });


  client.on('close', function() {
    // readyState is deprecated but we want to make
    // sure this isn't triggering an assert in lib/net.js
    // See issue #1069.
    assert.strictEqual('closed', client.readyState);

    // Confirming the buffer string is encoded in ASCII
    // and thus does NOT match the UTF8 string
    assert.notStrictEqual(buffer, messageUtf8);

    // Confirming the buffer string is encoded in ASCII
    // and thus does equal the ASCII string representation
    assert.strictEqual(buffer, messageAscii);

    server.close();
  });
});
