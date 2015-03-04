if (!process.features.tls_sni) {
  console.error('Skipping because node compiled without OpenSSL or ' +
                'with old OpenSSL version.');
  process.exit(0);
}

var common = require('../common'),
    assert = require('assert'),
    fs = require('fs');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var tls = require('tls');

function filenamePEM(n) {
  return require('path').join(common.fixturesDir, 'keys', n + '.pem');
}

function loadPEM(n) {
  return fs.readFileSync(filenamePEM(n));
}

var serverOptions = {
  key: loadPEM('agent2-key'),
  cert: loadPEM('agent2-cert')
};

var SNIContexts = {
  'a.example.com': {
    key: loadPEM('agent1-key'),
    cert: loadPEM('agent1-cert')
  },
  'asterisk.test.com': {
    key: loadPEM('agent3-key'),
    cert: loadPEM('agent3-cert')
  }
};

var serverPort = common.PORT;

var clientsOptions = [{
  port: serverPort,
  key: loadPEM('agent1-key'),
  cert: loadPEM('agent1-cert'),
  ca: [loadPEM('ca1-cert')],
  servername: 'a.example.com',
  rejectUnauthorized: false
}, {
  port: serverPort,
  key: loadPEM('agent2-key'),
  cert: loadPEM('agent2-cert'),
  ca: [loadPEM('ca2-cert')],
  servername: 'b.test.com',
  rejectUnauthorized: false
}, {
  port: serverPort,
  key: loadPEM('agent2-key'),
  cert: loadPEM('agent2-cert'),
  ca: [loadPEM('ca2-cert')],
  servername: 'a.b.test.com',
  rejectUnauthorized: false
}, {
  port: serverPort,
  key: loadPEM('agent3-key'),
  cert: loadPEM('agent3-cert'),
  ca: [loadPEM('ca1-cert')],
  servername: 'c.wrong.com',
  rejectUnauthorized: false
}];

var serverResults = [],
    clientResults = [];

var server = tls.createServer(serverOptions, function(c) {
  serverResults.push(c.servername);
});

server.addContext('a.example.com', SNIContexts['a.example.com']);
server.addContext('*.test.com', SNIContexts['asterisk.test.com']);

server.listen(serverPort, startTest);

function startTest() {
  var i = 0;
  function start() {
    // No options left
    if (i === clientsOptions.length)
      return server.close();

    var options = clientsOptions[i++];
    var client = tls.connect(options, function() {
      clientResults.push(
        client.authorizationError &&
        /Hostname\/IP doesn't/.test(client.authorizationError));
      client.destroy();

      // Continue
      start();
    });
  };

  start();
}

process.on('exit', function() {
  assert.deepEqual(serverResults, ['a.example.com', 'b.test.com',
                                   'a.b.test.com', 'c.wrong.com']);
  assert.deepEqual(clientResults, [true, true, false, false]);
});
