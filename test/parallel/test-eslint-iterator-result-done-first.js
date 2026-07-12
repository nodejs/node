'use strict';

const common = require('../common');
if ((!common.hasCrypto) || (!common.hasIntl)) {
  common.skip('ESLint tests require crypto and Intl');
}

common.skipIfEslintMissing();

const { RuleTester } = require('../../tools/eslint/node_modules/eslint');
const rule = require('../../tools/eslint-rules/iterator-result-done-first');

const message = 'Iterator result objects should place `done` before `value`.';

new RuleTester().run('iterator-result-done-first', rule, {
  valid: [
    'function next() { return { done: true, value: undefined }; }',
    'function next() { return { __proto__: null, done: false, value: chunk }; }',
    'function next() { return { done, value }; }',
    'function next() { return { value }; }',
    'function next() { return { done }; }',
    'function next() { return { value: 1, other: 2 }; }',
    'function next() { return { [value]: 1, done: true }; }',
    'function next() { return { value: 1, [done]: true }; }',
    'function next() { return { "done": true, "value": undefined }; }',
    'function next() { return { ["done"]: true, ["value"]: undefined }; }',
  ],
  invalid: [
    {
      code: 'function next() { return { value: undefined, done: true }; }',
      errors: [{ message }],
      output: 'function next() { return { done: true, value: undefined }; }',
    },
    {
      code: 'function next() { return { __proto__: null, value: chunk, done: false }; }',
      errors: [{ message }],
      output: 'function next() { return { __proto__: null, done: false, value: chunk }; }',
    },
    {
      code: 'function next() { return { value, done }; }',
      errors: [{ message }],
      output: 'function next() { return { done, value }; }',
    },
    {
      code: 'function next() { return { "value": undefined, "done": true }; }',
      errors: [{ message }],
      output: 'function next() { return { "done": true, "value": undefined }; }',
    },
    {
      code: 'function next() { return { ["value"]: undefined, ["done"]: true }; }',
      errors: [{ message }],
      output: 'function next() { return { ["done"]: true, ["value"]: undefined }; }',
    },
    {
      code: 'function next() { return { value: result, extra: true, done: false }; }',
      errors: [{ message }],
      output: 'function next() { return { done: false, extra: true, value: result }; }',
    },
  ],
});
