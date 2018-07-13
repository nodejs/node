'use strict';
// Flags: --expose-gc
const common = require('../common');

{
  const gcListener = { ongc: common.mustCall() };
  common.onGC({}, gcListener);
  global.gc();
}

{
  const gcListener = { ongc: common.mustNotCall() };
  common.onGC(process, gcListener);
  global.gc();
}
