'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

assert.throws(function() {
  tls.createSecureContext({ secureProtocol: 'blargh' });
}, /Unknown method/);

assert.throws(function() {
  tls.createSecureContext({ secureProtocol: 'SSLv2_method' });
}, /SSLv2 methods disabled/);

assert.throws(function() {
  tls.createSecureContext({ secureProtocol: 'SSLv2_client_method' });
}, /SSLv2 methods disabled/);

assert.throws(function() {
  tls.createSecureContext({ secureProtocol: 'SSLv2_server_method' });
}, /SSLv2 methods disabled/);

assert.throws(function() {
  tls.createSecureContext({ secureProtocol: 'SSLv3_method' });
}, /SSLv3 methods disabled/);

assert.throws(function() {
  tls.createSecureContext({ secureProtocol: 'SSLv3_client_method' });
}, /SSLv3 methods disabled/);

assert.throws(function() {
  tls.createSecureContext({ secureProtocol: 'SSLv3_server_method' });
}, /SSLv3 methods disabled/);

// Note that SSLv2 and SSLv3 are disallowed but SSLv2_method and friends are
// still accepted.  They are OpenSSL's way of saying that all known protocols
// are supported unless explicitly disabled (which we do for SSLv2 and SSLv3.)
tls.createSecureContext({ secureProtocol: 'SSLv23_method' });
tls.createSecureContext({ secureProtocol: 'SSLv23_client_method' });
tls.createSecureContext({ secureProtocol: 'SSLv23_server_method' });
tls.createSecureContext({ secureProtocol: 'TLSv1_method' });
tls.createSecureContext({ secureProtocol: 'TLSv1_client_method' });
tls.createSecureContext({ secureProtocol: 'TLSv1_server_method' });
tls.createSecureContext({ secureProtocol: 'TLSv1_1_method' });
tls.createSecureContext({ secureProtocol: 'TLSv1_1_client_method' });
tls.createSecureContext({ secureProtocol: 'TLSv1_1_server_method' });
tls.createSecureContext({ secureProtocol: 'TLSv1_2_method' });
tls.createSecureContext({ secureProtocol: 'TLSv1_2_client_method' });
tls.createSecureContext({ secureProtocol: 'TLSv1_2_server_method' });
