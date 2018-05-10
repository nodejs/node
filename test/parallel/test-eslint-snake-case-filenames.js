'use strict';

const common = require('../common');

common.skipIfEslintMissing();

const RuleTester = require('../../tools/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/snake-case-filenames');

const message = 'Filename is not in snake_case.';
const code = 'var foo = "bar"';

new RuleTester().run('snake-case-filenames', rule, {
  valid: [
    {
      code,
      filename: 'snake-case-filename.js'
    }
  ],
  invalid: [
    {
      code,
      filename: 'camelCaseFilename.js',
      errors: [{ message }]
    }
  ]
});
