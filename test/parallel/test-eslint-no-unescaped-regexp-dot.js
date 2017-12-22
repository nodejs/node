'use strict';

require('../common');

const RuleTester = require('../../tools/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/no-unescaped-regexp-dot');

new RuleTester().run('no-unescaped-regexp-dot', rule, {
  valid: [
    '/foo/',
    String.raw`/foo\./`,
    '/.+/',
    '/.*/',
    '/.?/',
    '/.{5}/',
    String.raw`/\\\./`
  ],
  invalid: [
    {
      code: '/./',
      errors: [{ message: 'Unescaped dot character in regular expression' }]
    },
    {
      code: String.raw`/\\./`,
      errors: [{ message: 'Unescaped dot character in regular expression' }]
    }
  ]
});
