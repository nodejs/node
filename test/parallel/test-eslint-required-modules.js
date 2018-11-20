'use strict';

const common = require('../common');

common.skipIfEslintMissing();

const RuleTester = require('../../tools/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/required-modules');

new RuleTester().run('required-modules', rule, {
  valid: [
    {
      code: 'require("common")',
      options: ['common']
    },
    {
      code: 'foo',
      options: []
    },
  ],
  invalid: [
    {
      code: 'foo',
      options: ['common'],
      errors: [{ message: 'Mandatory module "common" must be loaded.' }]
    },
    {
      code: 'require("somethingElse")',
      options: ['common'],
      errors: [{ message: 'Mandatory module "common" must be loaded.' }]
    }
  ]
});
