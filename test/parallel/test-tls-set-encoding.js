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

const message = 'hello world\n';


const server = tls.Server(options, common.mustCall(function(socket) {
  socket.end(message);
}));


server.listen(0, function() {
  const client = tls.connect({
    port: this.address().port,
    rejectUnauthorized: false
  });

  let buffer = '';

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
