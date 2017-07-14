'use strict';
const common = require('../common');
const http = require('http');

// sending `agent: false` when `port: null` is also passed in (i.e. the result
// of a `url.parse()` call with the default port used, 80 or 443), should not
// result in an assertion error...
const opts = {
  host: '127.0.0.1',
  port: null,
  path: '/',
  method: 'GET',
  agent: false
};

// we just want an "error" (no local HTTP server on port 80) or "response"
// to happen (user happens ot have HTTP server running on port 80).
// As long as the process doesn't crash from a C++ assertion then we're good.
const req = http.request(opts);

// Will be called by either the response event or error event, not both
const oneResponse = common.mustCall();
req.on('response', oneResponse);
req.on('error', oneResponse);
req.end();
