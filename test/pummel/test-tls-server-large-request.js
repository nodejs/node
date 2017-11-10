'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fs = require('fs');
const stream = require('stream');

const request = Buffer.from(new Array(1024 * 256).join('ABCD')); // 1mb

const options = {
  key: fs.readFileSync(`${common.fixturesDir}/keys/agent1-key.pem`),
  cert: fs.readFileSync(`${common.fixturesDir}/keys/agent1-cert.pem`)
};

class Mediator extends stream.Writable {
  constructor() {
    super();
    this.buf = '';
  }

  _write(data, enc, cb) {
    this.buf += data;
    setTimeout(cb, 0);

    if (this.buf.length >= request.length) {
      assert.strictEqual(this.buf, request.toString());
      server.close();
    }
  }
}

const mediator = new Mediator();

let server = tls.Server(options, common.mustCall(function(socket) {
  socket.pipe(mediator);
}));

server.listen(common.PORT, common.mustCall(function() {
  const client1 = tls.connect({
    port: common.PORT,
    rejectUnauthorized: false
  }, common.mustCall(function() {
    client1.end(request);
  }));
}));
