'use strict';

const async_wrap = process.binding('async_wrap');
const uv = process.binding('uv');
const assert = require('assert');
const common = require('../common');
const dgram = require('dgram');
const fs = require('fs');
const net = require('net');
const tls = require('tls');
const providers = Object.keys(async_wrap.Providers);
var flags = 0;

// Make sure all asserts have run at least once.
process.on('exit', () => assert.equal(flags, 0b111));

function init(id, provider) {
  this._external;  // Test will abort if nullptr isn't properly checked.
  switch (providers[provider]) {
    case 'TCPWRAP':
      assert.equal(this.fd, uv.UV_EINVAL);
      flags |= 0b1;
      break;
    case 'TLSWRAP':
      assert.equal(this.fd, uv.UV_EINVAL);
      flags |= 0b10;
      break;
    case 'UDPWRAP':
      assert.equal(this.fd, uv.UV_EBADF);
      flags |= 0b100;
      break;
  }
}

async_wrap.setupHooks({ init });
async_wrap.enable();

const checkTLS = common.mustCall(function checkTLS() {
  const options = {
    key: fs.readFileSync(common.fixturesDir + '/keys/ec-key.pem'),
    cert: fs.readFileSync(common.fixturesDir + '/keys/ec-cert.pem')
  };
  const server = tls.createServer(options, () => {})
    .listen(0, function() {
      const connectOpts = { rejectUnauthorized: false };
      tls.connect(this.address().port, connectOpts, function() {
        this.destroy();
        server.close();
      });
    });
});

const checkTCP = common.mustCall(function checkTCP() {
  net.createServer(() => {}).listen(0, function() {
    this.close(checkTLS);
  });
});

dgram.createSocket('udp4').close(checkTCP);
