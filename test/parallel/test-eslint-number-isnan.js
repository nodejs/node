'use strict';

require('../common');

const RuleTester = require('../../tools/eslint').RuleTester;
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
