'use strict';

var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var https = require('https');


// This is an end-to-end test of the HTTPS module.
// It ensures:
// * That `https.get()` accepts string URLs
// * That we can interact with real websites with valid certs
// * That we accept several types of unusual, but valid certs
// * That we correctly reject several types of invalid certs
var numResponses = 0;
var numRequests = 0;

// Basic functionality
expectSuccess('https://badssl.com');
expectSuccess('https://dh2048.badssl.com');
expectSuccess('https://sha256.badssl.com');
expectSuccess('https://1000-sans.badssl.com');

// Unusual or bad, but valid. Accepted by both node and Chrome.
expectSuccess('https://rsa8192.badssl.com');
expectSuccess('https://very.badssl.com');
expectSuccess('https://dh1024.badssl.com');
expectSuccess('https://sha1-2017.badssl.com');

// ACCEPTED by node but REJECTED by Chrome (!)
expectSuccess('https://pinning-test.badssl.com');

// REJECTED by node but ACCEPTED by Chrome (!)
expectFailure('https://incomplete-chain.badssl.com');

// Invalid. Rejected by both node and Chrome
expectFailure('https://expired.badssl.com');
expectFailure('https://wrong.host.badssl.com');
expectFailure('https://self-signed.badssl.com');
expectFailure('https://rc4.badssl.com');
expectFailure('https://dh480.badssl.com');
expectFailure('https://subdomain.preloaded-hsts.badssl.com');
expectFailure('https://superfish.badssl.com');
expectFailure('https://edellroot.badssl.com');
expectFailure('https://dsdtestprovider.badssl.com');

function expectSuccess(url) {
  expect(url, true);
}

function expectFailure(url) {
  expect(url, false);
}

function expect(url, shouldSucceed) {
  numRequests++;
  console.log('Requesting %s', url);
  https.get(url, function(res) {
    numResponses++;
    console.log('Got response for %s', url);
    assert(shouldSucceed, 'Expected an SSL error but got a response: ' + url);
  }).on('error', function(err) {
    numResponses++;
    console.log('Got error for %s: %o', url, err.message.slice(0, 40));
    assert(!shouldSucceed, 'Expected an response but got an SSL error: ' + url +
      '\n\t' + err);
  });
}

process.on('exit', function() {
  assert(numResponses === numRequests,
    'Made ' + numRequests + ' requests but got ' + numResponses + ' responses');
});
