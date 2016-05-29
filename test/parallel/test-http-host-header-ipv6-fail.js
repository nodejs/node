'use strict';
/*
 * When using the object form of http.request and using an IPv6 address
 * as a hostname, and using a non-standard port, the Host header
 * is improperly formatted.
 * Issue: https://github.com/nodejs/node/issues/5308
 * As per https://tools.ietf.org/html/rfc7230#section-5.4 and
 * https://tools.ietf.org/html/rfc3986#section-3.2.2
 * the IPv6 address should be enclosed in square brackets
 */

const common = require('../common');
const assert = require('assert');
const http = require('http');

const hostname = '::1';

function httpreq() {
  var req = http.request({
    host: hostname,
    port: server.address().port,
    path: '/',
    method: 'GET'
  });
  req.end();
}

if (!common.hasIPv6) {
  console.error('Skipping test, no IPv6 support');
  return;
}

const server = http.createServer(common.mustCall(function(req, res) {
  assert.ok(req.headers.host, `[${hostname}]`);
  res.end();
  server.close(true);
}));

server.listen(0, hostname, () => httpreq());
