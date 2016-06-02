'use strict';
// Test for authorized access to the server which has a cross root
// certification between Starfield Class 2 and ValiCert Class 2

var common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

var tls = require('tls');
var socket = tls.connect(443, 'address.melissadata.net', function() {
  socket.resume();
  socket.destroy();
});
