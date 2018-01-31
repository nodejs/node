'use strict';
const common = require('../common');
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
  common.expectsError(
    () => net.createConnection({ path: {} }),
    { code: 'ERR_INVALID_ARG_TYPE' }
  );
}
