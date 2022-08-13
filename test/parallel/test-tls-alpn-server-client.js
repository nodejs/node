'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { spawn } = require('child_process');
const tls = require('tls');
const fixtures = require('../common/fixtures');

function loadPEM(n) {
  return fixtures.readKey(`${n}.pem`);
}

const serverIP = common.localhostIPv4;

function checkResults(result, expected) {
  assert.strictEqual(result.server.ALPN, expected.server.ALPN);
  assert.strictEqual(result.client.ALPN, expected.client.ALPN);
}

function runTest(clientsOptions, serverOptions, cb) {
  serverOptions.key = loadPEM('agent2-key');
  serverOptions.cert = loadPEM('agent2-cert');
  const results = [];
  let clientIndex = 0;
  let serverIndex = 0;
  const server = tls.createServer(serverOptions, function(c) {
    results[serverIndex++].server = { ALPN: c.alpnProtocol };
  });

  server.listen(0, serverIP, function() {
    connectClient(clientsOptions);
  });

  function connectClient(options) {
    const opt = options.shift();
    opt.port = server.address().port;
    opt.host = serverIP;
    opt.rejectUnauthorized = false;

    results[clientIndex] = {};
    const client = tls.connect(opt, function() {
      results[clientIndex].client = { ALPN: client.alpnProtocol };
      client.end();
      if (options.length) {
        clientIndex++;
        connectClient(options);
      } else {
        server.close();
        server.on('close', () => {
          cb(results);
        });
      }
    });
  }

}

// Server: ALPN, Client: ALPN
function Test1() {
  const serverOptions = {
    ALPNProtocols: ['a', 'b', 'c'],
  };

  const clientsOptions = [{
    ALPNProtocols: ['a', 'b', 'c'],
  }, {
    ALPNProtocols: ['c', 'b', 'e'],
  }, {
    ALPNProtocols: ['x', 'y', 'c'],
  }];

  runTest(clientsOptions, serverOptions, function(results) {
    // 'a' is selected by ALPN
    checkResults(results[0],
                 { server: { ALPN: 'a' },
                   client: { ALPN: 'a' } });
    // 'b' is selected by ALPN
    checkResults(results[1],
                 { server: { ALPN: 'b' },
                   client: { ALPN: 'b' } });
    // Nothing is selected by ALPN
    checkResults(results[2],
                 { server: { ALPN: 'c' },
                   client: { ALPN: 'c' } });
    // execute next test
    Test2();
  });
}

// Server: ALPN, Client: Nothing
function Test2() {
  const serverOptions = {
    ALPNProtocols: ['a', 'b', 'c'],
  };

  const clientsOptions = [{}, {}, {}];

  runTest(clientsOptions, serverOptions, function(results) {
    // Nothing is selected by ALPN
    checkResults(results[0],
                 { server: { ALPN: false },
                   client: { ALPN: false } });
    // Nothing is selected by ALPN
    checkResults(results[1],
                 { server: { ALPN: false },
                   client: { ALPN: false } });
    // Nothing is selected by ALPN
    checkResults(results[2],
                 { server: { ALPN: false },
                   client: { ALPN: false } });
    // execute next test
    Test3();
  });
}

// Server: Nothing, Client: ALPN
function Test3() {
  const serverOptions = {};

  const clientsOptions = [{
    ALPNrotocols: ['a', 'b', 'c'],
  }, {
    ALPNProtocols: ['c', 'b', 'e'],
  }, {
    ALPNProtocols: ['first-priority-unsupported', 'x', 'y'],
  }];

  runTest(clientsOptions, serverOptions, function(results) {
    // nothing is selected
    checkResults(results[0], { server: { ALPN: false },
                               client: { ALPN: false } });
    // nothing is selected
    checkResults(results[1], { server: { ALPN: false },
                               client: { ALPN: false } });
    // nothing is selected
    checkResults(results[2],
                 { server: { ALPN: false },
                   client: { ALPN: false } });
    // execute next test
    Test4();
  });
}

// Server: Nothing, Client: Nothing
function Test4() {
  const serverOptions = {};

  const clientsOptions = [{}, {}, {}];

  runTest(clientsOptions, serverOptions, function(results) {
    // nothing is selected
    checkResults(results[0], { server: { ALPN: false },
                               client: { ALPN: false } });
    // nothing is selected
    checkResults(results[1], { server: { ALPN: false },
                               client: { ALPN: false } });
    // nothing is selected
    checkResults(results[2],
                 { server: { ALPN: false },
                   client: { ALPN: false } });
  });

  TestFatalAlert();
}

function TestFatalAlert() {
  const server = tls.createServer({
    ALPNProtocols: ['foo'],
    key: loadPEM('agent2-key'),
    cert: loadPEM('agent2-cert')
  }, common.mustNotCall());

  server.listen(0, serverIP, common.mustCall(() => {
    const { port } = server.address();

    // The Node.js client will just report ECONNRESET because the connection
    // is severed before the TLS handshake completes.
    tls.connect({
      host: serverIP,
      port,
      rejectUnauthorized: false,
      ALPNProtocols: ['bar']
    }, common.mustNotCall()).on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ECONNRESET');

      // OpenSSL's s_client should output the TLS alert number, which is 120
      // for the 'no_application_protocol' alert.
      const { opensslCli } = common;
      if (opensslCli) {
        const addr = `${serverIP}:${port}`;
        let stderr = '';
        spawn(opensslCli, ['s_client', '--alpn', 'bar', addr], {
          stdio: ['ignore', 'ignore', 'pipe']
        }).stderr
          .setEncoding('utf8')
          .on('data', (chunk) => stderr += chunk)
          .on('close', common.mustCall(() => {
            assert.match(stderr, /SSL alert number 120/);
            server.close();
          }));
      } else {
        server.close();
      }
    }));
  }));
}

Test1();
