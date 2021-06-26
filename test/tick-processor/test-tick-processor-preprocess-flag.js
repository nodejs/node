'use strict';
const common = require('../common');
const { isCPPSymbolsNotMapped } = require('./util');

if (isCPPSymbolsNotMapped) {
  common.skip('C++ symbols are not mapped for this os.');
}

const base = require('./tick-processor-base.js');

base.runTest({
  pattern: /^{/,
  code: `function f() {
           require('vm').createContext({});
           setImmediate(function() { f(); });
         };
         f();`,
  profProcessFlags: ['--preprocess']
});
