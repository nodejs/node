'use strict';
const common = require('../common');

if (!common.enoughTestCpu)
  common.skip('test is CPU-intensive');

if (common.isWindows ||
    common.isSunOS ||
    common.isAIX ||
    common.isLinuxPPCBE ||
    common.isFreeBSD)
  common.skip('C++ symbols are not mapped for this os.');

const base = require('./tick-processor-base.js');

base.runTest({
  pattern: /Builtin_DateNow/,
  code: `function f() {
           this.ts = Date.now();
           setImmediate(function() { new f(); });
         };
         f();`
});
