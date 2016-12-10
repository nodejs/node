'use strict';
var common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var https = require('https');

var http = require('http');

https.get('https://www.google.com/', common.mustCall(function(res) {
  res.resume();
}));

http.get('http://www.google.com/', common.mustCall(function(res) {
  res.resume();
}));
