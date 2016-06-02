'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');
var net = require('net');

var success = false;

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
