'use strict';
const common = require('../common');
const { isCPPSymbolsNotMapped } = require('./util');

if (!common.enoughTestCpu)
  common.skip('test is CPU-intensive');

if (isCPPSymbolsNotMapped) {
  common.skip('C++ symbols are not mapped for this os.');
}

const base = require('./tick-processor-base.js');

base.runTest({
  pattern: /Builtin_DateNow/,
  code: `function f() {
           this.ts = Date.now();
           setImmediate(function() { new f(); });
         };
         f();`
});
