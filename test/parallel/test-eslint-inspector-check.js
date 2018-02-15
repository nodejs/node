'use strict';

const common = require('../common');

common.skipIfEslintMissing();

const RuleTester = require('../../tools/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/inspector-check');

const message = 'Please add a skipIfInspectorDisabled() call to allow this ' +
                'test to be skippped when Node is built ' +
                '\'--without-inspector\'.';

new RuleTester().run('inspector-check', rule, {
  valid: [
    'foo;',
    'common.skipIfInspectorDisabled(); require("inspector");'
  ],
  invalid: [
    {
      code: 'require("inspector")',
      errors: [{ message }]
    }
  ]
});
