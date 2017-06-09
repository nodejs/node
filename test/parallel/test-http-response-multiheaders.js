'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');

// Test that certain response header fields do not repeat.
// 'content-length' should also be in this list but it is
// handled differently because multiple content-lengths are
// an error (see test-http-response-multi-content-length.js).
const norepeat = [
  'content-type',
  'user-agent',
  'referer',
  'host',
  'authorization',
  'proxy-authorization',
  'if-modified-since',
  'if-unmodified-since',
  'from',
  'location',
  'max-forwards',
  'retry-after',
  'etag',
  'last-modified',
  'server',
  'age',
  'expires'
];

const server = http.createServer(function(req, res) {
  const num = req.headers['x-num'];
  if (num === '1') {
    for (const name of norepeat) {
      res.setHeader(name, ['A', 'B']);
    }
    res.setHeader('X-A', ['A', 'B']);
  } else if (num === '2') {
    const headers = {};
    for (const name of norepeat) {
      headers[name] = ['A', 'B'];
    }
    headers['X-A'] = ['A', 'B'];
    res.writeHead(200, headers);
  }
  res.end('ok');
});

server.listen(0, common.mustCall(function() {
  let count = 0;
  for (let n = 1; n <= 2; n++) {
    // this runs twice, the first time, the server will use
    // setHeader, the second time it uses writeHead. The
    // result on the client side should be the same in
    // either case -- only the first instance of the header
    // value should be reported for the header fields listed
    // in the norepeat array.
    http.get(
      {port: this.address().port, headers: {'x-num': n}},
      common.mustCall(function(res) {
        if (++count === 2) server.close();
        for (const name of norepeat) {
          assert.strictEqual(res.headers[name], 'A');
        }
        assert.strictEqual(res.headers['x-a'], 'A, B');
      })
    );
  }
}));
