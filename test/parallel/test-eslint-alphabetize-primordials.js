'use strict';

const common = require('../common');
if ((!common.hasCrypto) || (!common.hasIntl)) {
  common.skip('ESLint tests require crypto and Intl');
}

common.skipIfEslintMissing();

const RuleTester = require('../../tools/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/alphabetize-primordials');

new RuleTester({
  parserOptions: { ecmaVersion: 6 },
  env: { es6: true }
})
  .run('alphabetize-primordials', rule, {
    valid: [
      'new Array()',
      '"use strict";const {Array} = primordials;',
      '"use strict";const {Array,Date} = primordials',
      '"use strict";const {Date,Array} = notPrimordials',
      '"use strict";const {Date,globalThis:{Array}} = primordials',
      '"use strict";const {BigInt,globalThis:{Array,Date,SharedArrayBuffer,parseInt}} = primordials',
      '"use strict";const {FunctionPrototypeBind,Uint32Array,globalThis:{SharedArrayBuffer}} = primordials',
      {
        code: '"use strict";const fs = require("fs");const {Array} = primordials',
        options: [{ enforceTopPosition: false }],
      },
    ],
    invalid: [
      {
        code: '"use strict";const fs = require("fs");const {Array} = primordials',
        errors: [{ message: /destructuring from primordials should be the first expression/ }],
      },
      {
        code: 'function fn() {"use strict";const {Array} = primordials}',
        errors: [{ message: /destructuring from primordials should be the first expression/ }],
      },
      {
        code: '"use strict";const {Date,Array} = primordials',
        errors: [{ message: /Date >= Array/ }],
      },
      {
        code: '"use strict";const {globalThis:{Date, Array}} = primordials',
        errors: [{ message: /Date >= Array/ }],
      },
    ]
  });
