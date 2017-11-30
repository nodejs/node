'use strict';

require('../common');

const RuleTester = require('../../tools/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/no-unescaped-regexp-dot');

new RuleTester().run('no-unescaped-regexp-dot', rule, {
  valid: [
    '/foo/',
    String.raw`/foo\./`,
    '/.+/',
    '/.*/',
    '/.?/',
    '/.{5}/',
    String.raw`/\\\./`,
    String.raw`/\\\\\./`
  ],
  invalid: [
    {
      code: String.raw`/./`,
      errors: [{ message: 'Unescaped dot character in regular expression' }],
      output: String.raw`/\./`
    },
    {
      code: String.raw`/\\./`,
      errors: [{ message: 'Unescaped dot character in regular expression' }],
      output: String.raw`/\\\./`
    },
    {
      code: String.raw`/\\\\./`,
      errors: [{ message: 'Unescaped dot character in regular expression' }],
      output: String.raw`/\\\\\./`
    }
  ]
});
