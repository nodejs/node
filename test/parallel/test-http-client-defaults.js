'use strict';
require('../common');
const assert = require('assert');
const ClientRequest = require('http').ClientRequest;

function noop() {}

{
  const req = new ClientRequest({ createConnection: noop });
  assert.strictEqual(req.path, '/');
  assert.strictEqual(req.method, 'GET');
}

{
  const req = new ClientRequest({ method: '', createConnection: noop });
  assert.strictEqual(req.path, '/');
  assert.strictEqual(req.method, 'GET');
}

{
  const req = new ClientRequest({ path: '', createConnection: noop });
  assert.strictEqual(req.path, '/');
  assert.strictEqual(req.method, 'GET');
}
