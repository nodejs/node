'use strict';
var common = require('../common');
var assert = require('assert');

var http = require('http');

if (common.hasCrypto) {
  var https = require('https');
} else {
  common.skip('missing crypto');
}

var host = '*'.repeat(256);

function do_not_call() {
  throw new Error('This function should not have been called.');
}

function test(mod) {

  // Bad host name should not throw an uncatchable exception.
  // Ensure that there is time to attach an error listener.
  var req1 = mod.get({host: host, port: 42}, do_not_call);
  req1.on('error', common.mustCall(function(err) {
    assert.equal(err.code, 'ENOTFOUND');
  }));
  // http.get() called req1.end() for us

  var req2 = mod.request({method: 'GET', host: host, port: 42}, do_not_call);
  req2.on('error', common.mustCall(function(err) {
    assert.equal(err.code, 'ENOTFOUND');
  }));
  req2.end();
}

if (common.hasCrypto) {
  test(https);
} else {
  common.skip('missing crypto');
}

test(http);
