'use strict';

require('../common');

const RuleTester = require('../../tools/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/crypto-check');

const message = 'Please add a hasCrypto check to allow this test to be ' +
                'skipped when Node is built "--without-ssl".';

new RuleTester().run('crypto-check', rule, {
  valid: [
    'foo',
    'crypto',
    `
      if (!common.hasCrypto) {
        common.skip();
      }
      require('crypto');
    `
  ],
  invalid: [
    {
      code: 'require("crypto")',
      errors: [{ message }]
    },
    {
      code: 'if (common.foo) {} require("crypto")',
      errors: [{ message }]
    }
  ]
});
