'use strict';

const common = require('../common');
const { get, createServer } = require('http');
const { pipeline } = require('stream');
const assert = require('assert');

let innerRequest;

// Http server
createServer(common.mustCall((req, res) => {
  res.writeHead(200);
  setTimeout(() => {
    innerRequest.abort();
    res.end('Hello World\n');
  }, 1000);
})).listen(3000);

// Proxy server
createServer(common.mustCall((req, res) => {
  get('http://127.0.0.1:3000', (inner) => {
    pipeline(inner, res, (err) => {
      assert(err);
    });
  });
}))
.listen(3001, common.mustCall(() => {
  innerRequest = get('http://127.0.0.1:3001');
}));
