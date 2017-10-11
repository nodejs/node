'use strict';

require('../common');

const RuleTester = require('../../tools/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/require-buffer');
const ruleTester = new RuleTester({
  parserOptions: { ecmaVersion: 6 },
  env: { node: true }
});

const message = "Use const Buffer = require('buffer').Buffer; " +
                'at the beginning of this file';

ruleTester.run('require-buffer', rule, {
  valid: [
    'foo',
    'const Buffer = require("Buffer"); Buffer;'
  ],
  invalid: [
    {
      code: 'Buffer;',
      errors: [{ message }]
    }
  ]
});
