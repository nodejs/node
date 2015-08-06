'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
const https = require('https');

const http = require('http');
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
