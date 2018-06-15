'use strict';

const common = require('../common');

common.skipIfEslintMissing();

const RuleTester = require('../../tools/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/snake-case-filenames');

const message = (filename) => `Filename ${filename} is not in snake_case.`;
const code = 'var foo = "bar"';

new RuleTester().run('snake-case-filenames', rule, {
  valid: [
    {
      code,
      filename: 'lib/snake_case_filename.js'
    }
  ],
  invalid: [
    {
      code,
      filename: 'lib/kebab-case-filename.js',
      errors: [{ ...message('kebab-case-filename.js') }]
    }
  ]
});
