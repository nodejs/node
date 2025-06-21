'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

{
  const fp = '/tmp/fadagagsdfgsdf';
  const c = net.connect(fp);

  c.on('connect', common.mustNotCall());
  c.on('error', common.expectsError({
    code: 'ENOENT',
    message: `connect ENOENT ${fp}`
  }));
}

{
  assert.throws(
    () => net.createConnection({ path: {} }),
    { code: 'ERR_INVALID_ARG_TYPE' }
  );
}
