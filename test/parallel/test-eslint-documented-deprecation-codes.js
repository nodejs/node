'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
common.skipIfEslintMissing();

const RuleTester = require('../../tools/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/documented-deprecation-codes');

const mdFile = 'doc/api/deprecations.md';

const invalidCode = 'UNDOCUMENTED INVALID CODE';

new RuleTester().run('documented-deprecation-codes', rule, {
  valid: [
    `
    deprecate(function() {
        return this.getHeaders();
      }, 'OutgoingMessage.prototype._headers is deprecated', 'DEP0066')
    `
  ],
  invalid: [
    {
      code: `
        deprecate(function foo(){}, 'bar', '${invalidCode}');
      `,
      errors: [
        {
          message: `"${invalidCode}" does not match the expected pattern`,
          line: 2
        },
        {
          message: `"${invalidCode}" is not documented in ${mdFile}`,
          line: 2
        },
        {
          message:
            `${mdFile} does not have an anchor for "${invalidCode}"`,
          line: 2
        }
      ]
    }
  ]
});
