'use strict';

// Test that http.ClientRequest,prototype.destroy() returns `this`.
require('../common');

const assert = require('assert');
const http = require('http');
const clientRequest = new http.ClientRequest({ createConnection: () => {} });

assert.strictEqual(clientRequest.destroyed, false);
assert.strictEqual(clientRequest.destroy(), clientRequest);
assert.strictEqual(clientRequest.destroyed, true);
assert.strictEqual(clientRequest.destroy(), clientRequest);
