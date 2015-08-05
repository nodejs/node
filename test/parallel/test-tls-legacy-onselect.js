'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
const tls = require('tls');
const net = require('net');

const fs = require('fs');

var success = false;

function filenamePEM(n) {
  return require('path').join(common.fixturesDir, 'keys', n + '.pem');
}

function loadPEM(n) {
  return fs.readFileSync(filenamePEM(n));
}

var server = net.Server(function(raw) {
  var pair = tls.createSecurePair(null, true, false, false);
  pair.on('error', function() {});
  pair.ssl.setSNICallback(function() {
    raw.destroy();
    server.close();
    success = true;
  });
  require('_tls_legacy').pipe(pair, raw);
}).listen(common.PORT, function() {
  tls.connect({
    port: common.PORT,
    rejectUnauthorized: false,
    servername: 'server'
  }, function() {
  }).on('error', function() {
    // Just ignore
  });
});
process.on('exit', function() {
  assert(success);
});
