'use strict';

require('../common');

const RuleTester = require('../../tools/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/inspector-check');

const message = 'Please add a skipIfInspectorDisabled() call to allow this ' +
                'test to be skippped when Node is built ' +
                '\'--without-inspector\'.';

new RuleTester().run('inspector-check', rule, {
  valid: [
    'foo;',
    'require("common")\n' +
      'common.skipIfInspectorDisabled();\n' +
      'require("inspector")',
  ],
  invalid: [
    {
      code: 'require("common")\n' +
            'require("inspector")',
      errors: [{ message }],
      output: 'require("common")\n' +
              'common.skipIfInspectorDisabled();\n' +
              'require("inspector")',
    },
  ],
});
