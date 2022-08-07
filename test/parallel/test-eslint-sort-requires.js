'use strict';

const common = require('../common');
if ((!common.hasCrypto) || (!common.hasIntl)) {
  common.skip('ESLint tests require crypto and Intl');
}

common.skipIfEslintMissing();

const RuleTester = require('../../tools/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/sort-requires');

new RuleTester({
  parserOptions: { ecmaVersion: 6 },
})
  .run('sort-requires', rule, {
    valid: [
      "const A = require('path')",
      'const A = primordials',
      "const { A } = require('path')",
      'const { A } = primordials',
      "const { A, B } = require('path')",
      'const { A, B } = primordials',
      "const { B: a, A: b } = require('path')",
      'const { B: a, A: b } = primordials',
      `const {
  A,
  B,
  C,
} = require('path')`,
      `const {
  A,
  B,
  C,
} = primordials`,
    ],
    invalid: [
      {
        code: "const { B, A } = require('path')",
        errors: [{ message: 'A should occur before B' }],
        output: "const { A, B } = require('path')",
      },
      {
        code: 'const { B, A } = primordials',
        errors: [{ message: 'A should occur before B' }],
        output: 'const { A, B } = primordials',
      },
      {
        code: "const { A: b, B: a } = require('path')",
        errors: [{ message: 'a should occur before b' }],
        output: "const { B: a, A: b } = require('path')",
      },
      {
        code: 'const { A: b, B: a } = primordials',
        errors: [{ message: 'a should occur before b' }],
        output: 'const { B: a, A: b } = primordials',
      },
      {
        code: "const { C, B, A } = require('path')",
        errors: [{ message: 'B should occur before C' }],
        output: "const { A, B, C } = require('path')",
      },
      {
        code: 'const { C, B, A } = primordials',
        errors: [{ message: 'B should occur before C' }],
        output: 'const { A, B, C } = primordials',
      },
      // Test for line breaks
      {
        code: `const {
  C,
  B,
  A
} = require('path')`,
        errors: [{ message: 'B should occur before C' }],
        output: `const {
  A,
  B,
  C
} = require('path')`,
      },
      {
        code: `const {
  C,
  B,
  A
} = primordials`,
        errors: [{ message: 'B should occur before C' }],
        output: `const {
  A,
  B,
  C
} = primordials`,
      },
      // Test for comments
      // If there is a comment, this rule changes nothing.
      {
        code: `const {
  // comment for B
  B,
  A
} = require('path')`,
        errors: [{ message: 'A should occur before B' }],
        output: null,
      },
      {
        code: `const {
  // comment for B
  B,
  A
} = primordials`,
        errors: [{ message: 'A should occur before B' }],
        output: null,
      },
    ]
  });
