'use strict';
const common = require('../common');
if ((!common.hasCrypto) || (!common.hasIntl)) {
  common.skip('ESLint tests require crypto and Intl');
}
common.skipIfEslintMissing();

const RuleTester = require('../../tools/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/async-iife-no-unused-result');

const message = 'The result of an immediately-invoked async function needs ' +
  'to be used (e.g. with `.then(common.mustCall())`)';

const tester = new RuleTester({ parserOptions: { ecmaVersion: 8 } });
tester.run('async-iife-no-unused-result', rule, {
  valid: [
    '(() => {})()',
    '(async () => {})',
    '(async () => {})().then()',
    '(async () => {})().catch()',
    '(function () {})()',
    '(async function () {})',
    '(async function () {})().then()',
    '(async function () {})().catch()',
  ],
  invalid: [
    {
      code: '(async () => {})()',
      errors: [{ message }],
      output: '(async () => {})()',
    },
    {
      code: '(async function() {})()',
      errors: [{ message }],
      output: '(async function() {})()',
    },
    {
      code: "const common = require('../common');(async () => {})()",
      errors: [{ message }],
      output: "const common = require('../common');(async () => {})()" +
        '.then(common.mustCall())',
    },
    {
      code: "const common = require('../common');(async function() {})()",
      errors: [{ message }],
      output: "const common = require('../common');(async function() {})()" +
        '.then(common.mustCall())',
    },
  ]
});
