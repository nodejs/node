'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
common.skipIfEslintMissing();

const RuleTester = require('../../tools/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/alphabetize-errors');

new RuleTester().run('alphabetize-errors', rule, {
  valid: [
    `
      E('AAA', 'foo');
      E('BBB', 'bar');
      E('CCC', 'baz');
    `
  ],
  invalid: [
    {
      code: `
        E('BBB', 'bar');
        E('AAA', 'foo');
        E('CCC', 'baz');
      `,
      errors: [{ message: 'Out of ASCIIbetical order - BBB >= AAA', line: 3 }]
    }
  ]
});
