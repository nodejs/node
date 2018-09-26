'use strict';
require('../common');

const base = require('../common/tick-processor');

base.runTest({
  // If the C++ symbols are not detected by the OS, no JS is going to be
  // executed.
  pattern: /Builtin: ObjectKeys| \d    [0-6]\.\d%    [0-6]\.\d%  JavaScript/,
  code: `let add = 0;
         const obj = { a: 5 };
         function f() {
           for (var i = 0; i < 4e6; i++) {
             obj.a = i;
             add += Object.keys(obj).length;
           }
           return add;
         };
         f();`
});
