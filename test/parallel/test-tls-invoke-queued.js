'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fs = require('fs');
const tls = require('tls');

let received = '';

const server = tls.createServer({
  key: fs.readFileSync(`${common.fixturesDir}/keys/agent1-key.pem`),
  cert: fs.readFileSync(`${common.fixturesDir}/keys/agent1-cert.pem`)
}, common.mustCall(function(c) {
  c.write('hello ', null, common.mustCall(function() {
    c.write('world!', null, common.mustCall(function() {
      c.destroy();
    }));
    // Data on next _write() will be written but callback will not be invoked
    c.write(' gosh', null, common.mustNotCall());
  }));

  server.close();
})).listen(0, common.mustCall(function() {
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
