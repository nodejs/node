'use strict';

const common = require('../common');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: node compiled without crypto.');
  return;
}

const assert = require('assert');
const tls = require('tls');
const fs = require('fs');
const path = require('path');

const keyDir = path.join(common.fixturesDir, 'keys');
const key = fs.readFileSync(path.join(keyDir, 'agent1-key.pem'));
const cert = fs.readFileSync(path.join(keyDir, 'agent1-cert.pem'));

const server = tls.createServer({
  key: key,
  cert: cert
}, function(c) {
  c.end();
}).listen(common.PORT, function() {
  var client = tls.connect({
    port: common.PORT,
    rejectUnauthorized: false
  }, common.mustCall(function() {
    client.destroy();
    if (!client._events.close)
      client._events.close = [];
    else if (!Array.isArray(client._events.close))
      client._events.close = [ client._events.close ];

    client._events.close.unshift(common.mustCall(function() {
      setImmediate(function() {
        // Make it crash on unpatched node.js
        var fd = client.ssl && client.ssl.fd;
        assert(client.ssl === null);
        assert(!fd);
      });
    }));
    server.close();
  }));
});
