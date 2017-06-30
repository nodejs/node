'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const https = require('https');

const http = require('http');

https.get('https://www.google.com/', common.mustCall(function(res) {
  res.resume();
}));

http.get('http://www.google.com/', common.mustCall(function(res) {
  res.resume();
}));
