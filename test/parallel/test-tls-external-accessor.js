'use strict';

const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

// Ensure accessing ._external doesn't hit an assert in the accessor method.
const tls = require('tls');
{
  const pctx = tls.createSecureContext().context;
  const cctx = Object.create(pctx);
  assert.throws(() => cctx._external, /incompatible receiver/);
  pctx._external;
}
{
  const pctx = tls.createSecurePair().credentials.context;
  const cctx = Object.create(pctx);
  assert.throws(() => cctx._external, /incompatible receiver/);
  pctx._external;
}
