'use strict';

/* eslint-disable no-template-curly-in-string */

const common = require('../common');

common.skipIfEslintMissing();

const RuleTester = require('../../tools/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/prefer-util-format-errors');

new RuleTester({ parserOptions: { ecmaVersion: 6 } })
  .run('prefer-util-format-errors', rule, {
    valid: [
      'E(\'ABC\', \'abc\');',
      'E(\'ABC\', (arg1, arg2) => `${arg2}${arg1}`);',
      'E(\'ABC\', (arg1, arg2) => `${arg1}{arg2.something}`);',
      'E(\'ABC\', (arg1, arg2) => fn(arg1, arg2));'
    ],
    invalid: [
      {
        code: 'E(\'ABC\', (arg1, arg2) => `${arg1}${arg2}`);',
        errors: [{
          message: 'Please use a printf-like formatted string that ' +
                   'util.format can consume.'
        }]
      }
    ]
  });
