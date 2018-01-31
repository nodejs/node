'use strict';

require('../common');

const RuleTester = require('../../tools/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/no-let-in-for-declaration');

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: 6 } });

const message = 'Use of `let` as the loop variable in a for-loop is ' +
                'not recommended. Please use `var` instead.';

ruleTester.run('no-let-in-for-declaration', rule, {
  valid: [
    'let foo;',
    'for (var foo = 1;;);',
    'for (foo = 1;;);',
    'for (;;);',
    'for (const foo of bar);',
    'for (var foo of bar);',
    'for (const foo in bar);',
    'for (var foo in bar);'
  ],
  invalid: [
    {
      code: 'for (let foo = 1;;);',
      output: 'for (var foo = 1;;);',
      errors: [{ message }]
    },
    {
      code: 'for (let foo in bar);',
      output: 'for (var foo in bar);',
      errors: [{ message }]
    },
    {
      code: 'for (let foo of bar);',
      output: 'for (var foo of bar);',
      errors: [{ message }]
    }
  ]
});
