'use strict';

// Flags: --max-old-space-size=10

require('../common');
const vm = require('vm');

const code = `console.log("${'hello world '.repeat(1e5)}");`;

for (let i = 0; i < 10000; i++) {
  vm.compileFunction(code, [], {
    importModuleDynamically: () => {},
  });
}
