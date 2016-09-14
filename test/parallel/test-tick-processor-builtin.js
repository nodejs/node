'use strict';
const common = require('../common');
const path = require('path');

// TODO(mhdawson) Currently the test-tick-processor functionality in V8
// depends on addresses being smaller than a full 64 bits.  Aix supports
// the full 64 bits and the result is that it does not process the
// addresses correctly and runs out of memory
// Disabling until we get a fix upstreamed into V8
if (common.isAix) {
  common.skip('Aix address range too big for scripts.');
  return;
}

const base = require(path.join(common.fixturesDir, 'tick-processor-base.js'));

if (common.isWindows ||
    common.isSunOS ||
    common.isAix ||
    common.isLinuxPPCBE ||
    common.isFreeBSD) {
  common.skip('C++ symbols are not mapped for this os.');
  return;
}

base.runTest({
  pattern: /Builtin_DateNow/,
  code: `function f() {
           this.ts = Date.now();
           setImmediate(function() { new f(); });
         };
         f();`
});
