'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');

// Ensure accessing ._external doesn't hit an assert in the accessor method.
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
