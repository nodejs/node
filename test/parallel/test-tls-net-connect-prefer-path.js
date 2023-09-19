'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');

// This tests that both tls and net will ignore host and port if path is
// provided.

if (!common.hasCrypto)
  common.skip('missing crypto');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const tls = require('tls');
const net = require('net');
const assert = require('assert');

function libName(lib) {
  return lib === net ? 'net' : 'tls';
}

function mkServer(lib, tcp, cb) {
  const handler = (socket) => {
    socket.write(`${libName(lib)}:${
      server.address().port || server.address()
    }`);
    socket.end();
  };
  const args = [handler];
  if (lib === tls) {
    args.unshift({
      cert: fixtures.readKey('rsa_cert.crt'),
      key: fixtures.readKey('rsa_private.pem')
    });
  }
  const server = lib.createServer(...args);
  server.listen(tcp ? 0 : common.PIPE, common.mustCall(() => cb(server)));
}

function testLib(lib, cb) {
  mkServer(lib, true, (tcpServer) => {
    mkServer(lib, false, (unixServer) => {
      const client = lib.connect({
        path: unixServer.address(),
        port: tcpServer.address().port,
        host: 'localhost',
        rejectUnauthorized: false
      }, () => {
        const bufs = [];
        client.on('data', common.mustCall((d) => {
          bufs.push(d);
        }));
        client.on('end', common.mustCall(() => {
          const resp = Buffer.concat(bufs).toString();
          assert.strictEqual(resp, `${libName(lib)}:${unixServer.address()}`);
          tcpServer.close();
          unixServer.close();
          cb();
        }));
      });
    });
  });
}

testLib(net, common.mustCall(() => testLib(tls, common.mustCall())));
