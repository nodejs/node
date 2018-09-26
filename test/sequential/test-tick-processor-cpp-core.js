'use strict';
require('../common');

const base = require('../common/tick-processor');

base.runTest({
  // If the C++ symbols are not mapped to the OS they can end up as `UNKNOWN` or
  // no JS is executed at all.
  pattern: /MakeContext|\d\d\.\d%  UNKNOWN|0    0\.0%    0\.0%  JavaScript|[7-9]\d\.\d%   [7-9]\d\.\d%  write/,
  code: `function f() {
           for (var i = 0; i < 5e2; i++) {
             require('vm').createContext({});
           }
         };
         f();`
});
