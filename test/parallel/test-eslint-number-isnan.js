'use strict';

const common = require('../common');

common.skipIfEslintMissing();

const RuleTester = require('../../tools/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/number-isnan');

const message = 'Please use Number.isNaN instead of the global isNaN function';

new RuleTester().run('number-isnan', rule, {
  valid: [
    'Number.isNaN()'
  ],
  invalid: [
    {
      code: 'isNaN()',
      errors: [{ message }]
    }
  ]
});
