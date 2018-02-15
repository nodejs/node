'use strict';

const common = require('../common');

common.skipIfEslintMissing();

const RuleTester = require('../../tools/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/prefer-assert-iferror');

new RuleTester().run('prefer-assert-iferror', rule, {
  valid: [
    'assert.ifError(err);',
    'if (err) throw somethingElse;',
    'throw err;',
    'if (err) { throw somethingElse; }'
  ],
  invalid: [
    {
      code: 'if (err) throw err;',
      errors: [{ message: 'Use assert.ifError(err) instead.' }]
    },
    {
      code: 'if (error) { throw error; }',
      errors: [{ message: 'Use assert.ifError(error) instead.' }]
    }
  ]
});
