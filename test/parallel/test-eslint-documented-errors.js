'use strict';

require('../common');

const RuleTester = require('../../tools/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/documented-errors');

const invalidCode = 'UNDOCUMENTED ERROR CODE';

new RuleTester().run('documented-errors', rule, {
  valid: [
    `
      E('ERR_ASSERTION', 'foo');
    `
  ],
  invalid: [
    {
      code: `
        E('${invalidCode}', 'bar');
      `,
      errors: [
        {
          message: `"${invalidCode}" is not documented in doc/api/errors.md`,
          line: 2
        },
        {
          message:
            `doc/api/errors.md does not have an anchor for "${invalidCode}"`,
          line: 2
        }
      ]
    }
  ]
});
