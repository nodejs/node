var common = require('../common');
var assert = require('assert');

var http = require('http');

if (common.hasCrypto) {
  var https = require('https');
} else {
  console.log('1..0 # Skipped: missing crypto');
}

var expected_bad_requests = 0;
var actual_bad_requests = 0;

var host = '********';
host += host;
host += host;
host += host;
host += host;
host += host;

function do_not_call() {
  throw new Error('This function should not have been called.');
}

function test(mod) {
  expected_bad_requests += 2;

  // Bad host name should not throw an uncatchable exception.
  // Ensure that there is time to attach an error listener.
  var req = mod.get({host: host, port: 42}, do_not_call);
  req.on('error', function(err) {
    assert.equal(err.code, 'ENOTFOUND');
    actual_bad_requests++;
  });
  // http.get() called req.end() for us

  var req = mod.request({method: 'GET', host: host, port: 42}, do_not_call);
  req.on('error', function(err) {
    assert.equal(err.code, 'ENOTFOUND');
    actual_bad_requests++;
  });
  req.end();
}

if (common.hasCrypto) {
  test(https);
} else {
  console.log('1..0 # Skipped: missing crypto');
}

test(http);

process.on('exit', function() {
  assert.equal(actual_bad_requests, expected_bad_requests);
});
