'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

common.skipIfEslintMissing();

const { RuleTester } = require('../../tools/node_modules/eslint');
const rule = require('../../tools/eslint-rules/no-duplicate-requires');

new RuleTester().run('no-duplicate-requires', rule, {
  valid: [
    {
      code: 'require("a"); require("b"); (function() { require("a"); });',
    },
    {
      code: 'require(a); require(a);',
    },
  ],
  invalid: [
    {
      code: 'require("a"); require("a");',
      errors: [{ message: '\'a\' require is duplicated.' }],
    },
  ],
});
