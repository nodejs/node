'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

common.skipIfEslintMissing();

const RuleTester = require('../../tools/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/eslint-check');

const message = 'Please add a skipIfEslintMissing() call to allow this ' +
                'test to be skipped when Node.js is built ' +
                'from a source tarball.';

new RuleTester().run('eslint-check', rule, {
  valid: [
    'foo;',
    'require("common")\n' +
      'common.skipIfEslintMissing();\n' +
      'require("../../tools/node_modules/eslint")'
  ],
  invalid: [
    {
      code: 'require("common")\n' +
            'require("../../tools/node_modules/eslint").RuleTester',
      errors: [{ message }],
      output: 'require("common")\n' +
              'common.skipIfEslintMissing();\n' +
              'require("../../tools/node_modules/eslint").RuleTester'
    }
  ]
});
