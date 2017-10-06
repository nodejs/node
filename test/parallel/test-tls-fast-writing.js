'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const assert = require('assert');
const tls = require('tls');

const options = { key: fixtures.readSync('test_key.pem'),
                  cert: fixtures.readSync('test_cert.pem'),
                  ca: [ fixtures.readSync('test_ca.pem') ] };

const server = tls.createServer(options, onconnection);
let gotChunk = false;
let gotDrain = false;

function onconnection(conn) {
  conn.on('data', function(c) {
    if (!gotChunk) {
      gotChunk = true;
      console.log('ok - got chunk');
    }

    // just some basic sanity checks.
    assert(c.length);
    assert(Buffer.isBuffer(c));

    if (gotDrain)
      process.exit(0);
  });
}

server.listen(0, function() {
  const chunk = Buffer.alloc(1024, 'x');
  const opt = { port: this.address().port, rejectUnauthorized: false };
  const conn = tls.connect(opt, function() {
    conn.on('drain', ondrain);
    write();
  });
  function ondrain() {
    if (!gotDrain) {
      gotDrain = true;
      console.log('ok - got drain');
    }
    if (gotChunk)
      process.exit(0);
    write();
  }
  function write() {
    // this needs to return false eventually
    while (false !== conn.write(chunk));
  }
});
