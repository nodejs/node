'use strict';

const assert = require('assert');
const common = require('../common');
const tls = require('tls');
const fs = require('fs');

const ca1 =
    fs.readFileSync(`${common.fixturesDir}/keys/ca1-cert.pem`, 'utf8');
const ca2 =
    fs.readFileSync(`${common.fixturesDir}/keys/ca2-cert.pem`, 'utf8');
const cert =
    fs.readFileSync(`${common.fixturesDir}/keys/agent3-cert.pem`, 'utf8');
const key =
    fs.readFileSync(`${common.fixturesDir}/keys/agent3-key.pem`, 'utf8');


function test(ca) {
  return new Promise(function(resolve, reject) {
    const server = tls.createServer(function(conn) {
      this.close();
      conn.end();
    });

    server.addContext('agent3', { ca, cert, key });

    const host = common.localhostIPv4;
    const port = common.PORT;
    server.listen(port, host, function() {
      var socket = tls.connect({ servername: 'agent3', host, port, ca });
      var socketError = false;
      socket.on('error', (e) => { socketError = e; });
      server.once('close',
                  (e) => {
                    return socketError ? reject(socketError) : resolve();
                  });
    });
  });
}

test(ca1 + '\n' + ca2)
  .then(test.bind(null, ca2 + '\n' + ca1))
  .then(test.bind(null, ca1 + ca2))
  .then(test.bind(null, ca2 + ca1))
  .then(test.bind(null, [ca1, ca2]))
  .then(test.bind(null, [ca2, ca1]));

process.on('unhandledRejection', function(reason) {
  assert.fail(null, null, reason);
});
