'use strict';
require('../common');
var assert = require('assert');
var http = require('http');

// sending `agent: false` when `port: null` is also passed in (i.e. the result
// of a `url.parse()` call with the default port used, 80 or 443), should not
// result in an assertion error...
var opts = {
  host: '127.0.0.1',
  port: null,
  path: '/',
  method: 'GET',
  agent: false
};

var good = false;
process.on('exit', function() {
  assert(good, 'expected either an "error" or "response" event');
});

// we just want an "error" (no local HTTP server on port 80) or "response"
// to happen (user happens ot have HTTP server running on port 80).
// As long as the process doesn't crash from a C++ assertion then we're good.
var req = http.request(opts);
req.on('response', function(res) {
  good = true;
});
req.on('error', function(err) {
  // an "error" event is ok, don't crash the process
  good = true;
});
req.end();
