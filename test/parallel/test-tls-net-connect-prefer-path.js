'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');

// This tests that both tls and net will ignore host and port if path is
// provided.

if (!common.hasCrypto)
  common.skip('missing crypto');

common.refreshTmpDir();

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
      cert: fixtures.readSync('test_cert.pem'),
      key: fixtures.readSync('test_key.pem')
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
          assert.strictEqual(`${libName(lib)}:${unixServer.address()}`, resp);
          tcpServer.close();
          unixServer.close();
          cb();
        }));
      });
    });
  });
}

testLib(net, common.mustCall(() => testLib(tls, common.mustCall())));
