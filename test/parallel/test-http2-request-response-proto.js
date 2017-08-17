// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const {
  Http2ServerRequest,
  Http2ServerResponse,
} = http2;

const protoRequest = Object.create(Http2ServerRequest.prototype);
const protoResponse = Object.create(Http2ServerResponse.prototype);

assert.strictEqual(protoRequest instanceof Http2ServerRequest, true);
assert.strictEqual(protoResponse instanceof Http2ServerResponse, true);
