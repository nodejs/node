'use strict';

const common = require('../common');
const assert = require('assert');
const ClientRequest = require('http').ClientRequest;

{
  const req = new ClientRequest({ createConnection: common.noop });
  assert.strictEqual(req.path, '/');
  assert.strictEqual(req.method, 'GET');
}

{
  const req = new ClientRequest({ method: '', createConnection: common.noop });
  assert.strictEqual(req.path, '/');
  assert.strictEqual(req.method, 'GET');
}

{
  const req = new ClientRequest({ path: '', createConnection: common.noop });
  assert.strictEqual(req.path, '/');
  assert.strictEqual(req.method, 'GET');
}
