'use strict';
const common = require('../common');

// TODO(mhdawson) Currently the test-tick-processor functionality in V8
// depends on addresses being smaller than a full 64 bits.  AIX supports
// the full 64 bits and the result is that it does not process the
// addresses correctly and runs out of memory
// Disabling until we get a fix upstreamed into V8
if (common.isAIX)
  common.skip('AIX address range too big for scripts.');

const base = require('./tick-processor-base.js');

// Unknown checked for to prevent flakiness, if pattern is not found,
// then a large number of unknown ticks should be present
base.runTest({
  pattern: /LazyCompile.*\[eval]:1|.*%  UNKNOWN/,
  code: `function f() {
           for (let i = 0; i < 1000000; i++) {
             i++;
           }
           setImmediate(function() { f(); });
         };
         f();`
});
