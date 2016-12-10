'use strict';
const common = require('../common');
const assert = require('assert');

const http = require('http');

if (common.hasCrypto) {
  var https = require('https');
} else {
  common.skip('missing crypto');
}

const host = '*'.repeat(256);

function do_not_call() {
  throw new Error('This function should not have been called.');
}

function test(mod) {

  // Bad host name should not throw an uncatchable exception.
  // Ensure that there is time to attach an error listener.
  const req1 = mod.get({host: host, port: 42}, do_not_call);
  req1.on('error', common.mustCall(function(err) {
    assert.strictEqual(err.code, 'ENOTFOUND');
  }));
  // http.get() called req1.end() for us

  const req2 = mod.request({method: 'GET', host: host, port: 42}, do_not_call);
  req2.on('error', common.mustCall(function(err) {
    assert.strictEqual(err.code, 'ENOTFOUND');
  }));
  req2.end();
}

if (common.hasCrypto) {
  test(https);
} else {
  common.skip('missing crypto');
}

test(http);
