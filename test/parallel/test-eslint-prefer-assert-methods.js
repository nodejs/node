'use strict';

const common = require('../common');

common.skipIfEslintMissing();

const RuleTester = require('../../tools/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/prefer-assert-methods');

new RuleTester().run('prefer-assert-methods', rule, {
  valid: [
    'assert.strictEqual(foo, bar)',
    'assert(foo === bar && baz)'
  ],
  invalid: [
    {
      code: 'assert(foo == bar)',
      errors: [{ message: "'assert.equal' should be used instead of '=='" }]
    },
    {
      code: 'assert(foo === bar)',
      errors: [{
        message: "'assert.strictEqual' should be used instead of '==='"
      }]
    },
    {
      code: 'assert(foo != bar)',
      errors: [{
        message: "'assert.notEqual' should be used instead of '!='"
      }]
    },
    {
      code: 'assert(foo !== bar)',
      errors: [{
        message: "'assert.notStrictEqual' should be used instead of '!=='"
      }]
    },
  ]
});
