'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

common.skipIfEslintMissing();

const RuleTester = require('../../tools/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/required-modules');

new RuleTester().run('required-modules', rule, {
  valid: [
    {
      code: 'require("common")',
      options: [{ common: 'common' }]
    },
    {
      code: 'foo',
      options: []
    },
    {
      code: 'require("common")',
      options: [{ common: 'common(/index\\.(m)?js)?$' }]
    },
    {
      code: 'require("common/index.js")',
      options: [{ common: 'common(/index\\.(m)?js)?$' }]
    },
  ],
  invalid: [
    {
      code: 'foo',
      options: [{ common: 'common' }],
      errors: [{ message: 'Mandatory module "common" must be loaded.' }]
    },
    {
      code: 'require("common/fixtures.js")',
      options: [{ common: 'common(/index\\.(m)?js)?$' }],
      errors: [{
        message:
          'Mandatory module "common" must be loaded.'
      }]
    },
    {
      code: 'require("somethingElse")',
      options: [{ common: 'common' }],
      errors: [{ message: 'Mandatory module "common" must be loaded.' }]
    }
  ]
});
