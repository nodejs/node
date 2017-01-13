'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');


let received = '';

const server = tls.createServer({
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
}).listen(0, common.mustCall(function() {
  const c = tls.connect(this.address().port, {
    rejectUnauthorized: false
  }, common.mustCall(function() {
    c.on('data', function(chunk) {
      received += chunk;
    });
    c.on('end', common.mustCall(function() {
      assert.strictEqual(received, 'hello world! gosh');
    }));
  }));
}));
