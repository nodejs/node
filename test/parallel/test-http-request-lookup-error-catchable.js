'use strict';
const common = require('../common');
const http = require('http');
const net = require('net');

// This test verifies that errors occurring synchronously during connection
// when using http.request with a custom lookup function and blockList
// can be caught by the error handler.
// Regression test for https://github.com/nodejs/node/issues/48771

// The issue occurs when:
// 1. http.request() is called with a custom synchronous lookup function
// 2. The lookup returns an IP that triggers a synchronous error (e.g., blockList)
// 3. The error is emitted before http's error handler is set up (via nextTick)
//
// The fix attaches socketErrorListener synchronously in onSocket so that
// socket errors are forwarded to the request before onSocketNT runs.

const blockList = new net.BlockList();
blockList.addAddress(common.localhostIPv4);

// Synchronous lookup that returns the blocked IP
const lookup = (_hostname, _options, callback) => {
  callback(null, common.localhostIPv4, 4);
};

const req = http.request({
  host: 'example.com',
  port: 80,
  lookup,
  family: 4, // Force IPv4 to use simple lookup path
  createConnection: (opts) => {
    // Pass blockList to trigger synchronous ERR_IP_BLOCKED error
    return net.createConnection({ ...opts, blockList });
  },
}, common.mustNotCall());

// This error handler must be called.
// Without the fix, the error would be emitted before http.request()
// returns, causing an unhandled 'error' event.
req.on('error', common.mustCall((err) => {
  if (err.code !== 'ERR_IP_BLOCKED') {
    throw new Error(`Expected ERR_IP_BLOCKED but got ${err.code}`);
  }
}));

req.end();
