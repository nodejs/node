'use strict';

const common = require('../common');
const assert = require('assert');
const ClientRequest = require('http').ClientRequest;

{
  const req = new ClientRequest(() => {});
  req.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ECONNREFUSED');
  }))
  assert.strictEqual(req.path, '/');
  assert.strictEqual(req.method, 'GET');
}
