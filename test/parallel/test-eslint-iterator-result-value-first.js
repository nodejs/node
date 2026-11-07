'use strict';

const common = require('../common');
if ((!common.hasCrypto) || (!common.hasIntl)) {
  common.skip('ESLint tests require crypto and Intl');
}

common.skipIfEslintMissing();

const { RuleTester } = require('../../tools/eslint/node_modules/eslint');
const rule = require('../../tools/eslint-rules/iterator-result-value-first');

const message = 'Iterator result objects should place `value` before `done`.';

new RuleTester().run('iterator-result-value-first', rule, {
  valid: [
    'function next() { return { value: undefined, done: true }; }',
    'function next() { return { __proto__: null, value: chunk, done: false }; }',
    'function next() { return { value, done }; }',
    'function next() { return { value }; }',
    'function next() { return { done }; }',
    'function next() { return { done: 1, other: 2 }; }',
    'function next() { return { [done]: 1, value: true }; }',
    'function next() { return { done: 1, [value]: true }; }',
    'function next() { return { "value": undefined, "done": true }; }',
    'function next() { return { ["value"]: undefined, ["done"]: true }; }',
  ],
  invalid: [
    {
      code: 'function next() { return { done: true, value: undefined }; }',
      errors: [{ message }],
      output: 'function next() { return { value: undefined, done: true }; }',
    },
    {
      code: 'function next() { return { __proto__: null, done: false, value: chunk }; }',
      errors: [{ message }],
      output: 'function next() { return { __proto__: null, value: chunk, done: false }; }',
    },
    {
      code: 'function next() { return { done, value }; }',
      errors: [{ message }],
      output: 'function next() { return { value, done }; }',
    },
    {
      code: 'function next() { return { "done": true, "value": undefined }; }',
      errors: [{ message }],
      output: 'function next() { return { "value": undefined, "done": true }; }',
    },
    {
      code: 'function next() { return { ["done"]: true, ["value"]: undefined }; }',
      errors: [{ message }],
      output: 'function next() { return { ["value"]: undefined, ["done"]: true }; }',
    },
    {
      code: 'function next() { return { done: false, extra: true, value: result }; }',
      errors: [{ message }],
      output: 'function next() { return { value: result, extra: true, done: false }; }',
    },
  ],
});
