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

const protoRequest = { __proto__: Http2ServerRequest.prototype };
const protoResponse = { __proto__: Http2ServerResponse.prototype };

assert.strictEqual(protoRequest instanceof Http2ServerRequest, true);
assert.strictEqual(protoResponse instanceof Http2ServerResponse, true);
