'use strict';
require('../common');

const base = require('../common/tick-processor');

base.runTest({
  pattern: /^{/,
  code: `function f() {
          for (var i = 0; i < 5e2; i++) {
            require('vm').createContext({});
          }
         };
         f();`,
  profProcessFlags: ['--preprocess']
});
