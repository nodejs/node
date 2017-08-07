// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const options = {};

const server = http2.createServer(options);

// options are defaulted but the options are not modified
assert.deepStrictEqual(Object.keys(options), []);

server.on('stream', common.mustCall((stream) => {
  const headers = {};
  const options = {};
  stream.respond(headers, options);

  // The headers are defaulted but the original object is not modified
  assert.deepStrictEqual(Object.keys(headers), []);

  // Options are defaulted but the original object is not modified
  assert.deepStrictEqual(Object.keys(options), []);

  stream.end();
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  const headers = {};
  const options = {};

  const req = client.request(headers, options);

  // The headers are defaulted but the original object is not modified
  assert.deepStrictEqual(Object.keys(headers), []);

  // Options are defaulted but the original object is not modified
  assert.deepStrictEqual(Object.keys(options), []);

  req.resume();
  req.on('end', common.mustCall(() => {
    server.close();
    client.destroy();
  }));
}));
