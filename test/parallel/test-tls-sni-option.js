'use strict';
const common = require('../common');
if (!process.features.tls_sni) {
  common.skip('node compiled without OpenSSL or ' +
              'with old OpenSSL version.');
  return;
}

const assert = require('assert');
const fs = require('fs');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
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
  cert: loadPEM('agent2-cert'),
  requestCert: true,
  rejectUnauthorized: false,
  SNICallback: function(servername, callback) {
    var context = SNIContexts[servername];

    // Just to test asynchronous callback
    setTimeout(function() {
      if (context) {
        if (context.emptyRegression)
          callback(null, {});
        else
          callback(null, tls.createSecureContext(context));
      } else {
        callback(null, null);
      }
    }, 100);
  }
};

var SNIContexts = {
  'a.example.com': {
    key: loadPEM('agent1-key'),
    cert: loadPEM('agent1-cert'),
    ca: [ loadPEM('ca2-cert') ]
  },
  'b.example.com': {
    key: loadPEM('agent3-key'),
    cert: loadPEM('agent3-cert')
  },
  'c.another.com': {
    emptyRegression: true
  }
};

var clientsOptions = [{
  port: undefined,
  key: loadPEM('agent1-key'),
  cert: loadPEM('agent1-cert'),
  ca: [loadPEM('ca1-cert')],
  servername: 'a.example.com',
  rejectUnauthorized: false
}, {
  port: undefined,
  key: loadPEM('agent4-key'),
  cert: loadPEM('agent4-cert'),
  ca: [loadPEM('ca1-cert')],
  servername: 'a.example.com',
  rejectUnauthorized: false
}, {
  port: undefined,
  key: loadPEM('agent2-key'),
  cert: loadPEM('agent2-cert'),
  ca: [loadPEM('ca2-cert')],
  servername: 'b.example.com',
  rejectUnauthorized: false
}, {
  port: undefined,
  key: loadPEM('agent3-key'),
  cert: loadPEM('agent3-cert'),
  ca: [loadPEM('ca1-cert')],
  servername: 'c.wrong.com',
  rejectUnauthorized: false
}, {
  port: undefined,
  key: loadPEM('agent3-key'),
  cert: loadPEM('agent3-cert'),
  ca: [loadPEM('ca1-cert')],
  servername: 'c.another.com',
  rejectUnauthorized: false
}];

const serverResults = [];
const clientResults = [];
const serverErrors = [];
const clientErrors = [];
let serverError;
let clientError;

var server = tls.createServer(serverOptions, function(c) {
  serverResults.push({ sni: c.servername, authorized: c.authorized });
});

server.on('tlsClientError', function(err) {
  serverResults.push(null);
  serverError = err.message;
});

server.listen(0, startTest);

function startTest() {
  function connectClient(i, callback) {
    var options = clientsOptions[i];
    clientError = null;
    serverError = null;

    options.port = server.address().port;
    var client = tls.connect(options, function() {
      clientResults.push(
          /Hostname\/IP doesn't/.test(client.authorizationError || ''));
      client.destroy();

      next();
    });

    client.on('error', function(err) {
      clientResults.push(false);
      clientError = err.message;
      next();
    });

    function next() {
      clientErrors.push(clientError);
      serverErrors.push(serverError);

      if (i === clientsOptions.length - 1)
        callback();
      else
        connectClient(i + 1, callback);
    }
  }

  connectClient(0, function() {
    server.close();
  });
}

process.on('exit', function() {
  assert.deepStrictEqual(serverResults, [
    { sni: 'a.example.com', authorized: false },
    { sni: 'a.example.com', authorized: true },
    { sni: 'b.example.com', authorized: false },
    { sni: 'c.wrong.com', authorized: false },
    null
  ]);
  assert.deepStrictEqual(clientResults, [true, true, true, false, false]);
  assert.deepStrictEqual(clientErrors, [
    null, null, null, null, 'socket hang up'
  ]);
  assert.deepStrictEqual(serverErrors, [
    null, null, null, null, 'Invalid SNI context'
  ]);
});
