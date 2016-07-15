'use strict';
var common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');
var net = require('net');

var server = net.Server(common.mustCall(function(raw) {
  var pair = tls.createSecurePair(null, true, false, false);
  pair.on('error', function() {});
  pair.ssl.setSNICallback(common.mustCall(function() {
    raw.destroy();
    server.close();
  }));
  require('_tls_legacy').pipe(pair, raw);
})).listen(0, function() {
  tls.connect({
    port: this.address().port,
    rejectUnauthorized: false,
    servername: 'server'
  }, function() {
  }).on('error', function() {
    // Just ignore
  });
});
