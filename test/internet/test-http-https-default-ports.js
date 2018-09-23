'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var https = require('https');

var http = require('http');
var gotHttpsResp = false;
var gotHttpResp = false;

process.on('exit', function() {
  assert(gotHttpsResp);
  assert(gotHttpResp);
  console.log('ok');
});

https.get('https://www.google.com/', function(res) {
  gotHttpsResp = true;
  res.resume();
});

http.get('http://www.google.com/', function(res) {
  gotHttpResp = true;
  res.resume();
});
