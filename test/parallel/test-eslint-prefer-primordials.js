'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

common.skipIfEslintMissing();

const RuleTester = require('../../tools/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/prefer-primordials');

new RuleTester({
  parserOptions: { ecmaVersion: 6 },
  env: { es6: true }
})
  .run('prefer-primordials', rule, {
    valid: [
      'new Array()',
      'JSON.stringify({})',
      'class A { *[Symbol.iterator] () { yield "a"; } }',
      'const a = { *[Symbol.iterator] () { yield "a"; } }',
      'Object.defineProperty(o, Symbol.toStringTag, { value: "o" })',
      'parseInt("10")',
      `
        const { Reflect } = primordials;
        module.exports = function() {
          const { ownKeys } = Reflect;
        }
      `,
      {
        code: 'const { Array } = primordials; new Array()',
        options: [{ name: 'Array' }]
      },
      {
        code: 'const { JSONStringify } = primordials; JSONStringify({})',
        options: [{ name: 'JSON' }]
      },
      {
        code: 'const { SymbolFor } = primordials; SymbolFor("xxx")',
        options: [{ name: 'Symbol' }]
      },
      {
        code: `
          const { SymbolIterator } = primordials;
          class A { *[SymbolIterator] () { yield "a"; } }
        `,
        options: [{ name: 'Symbol' }]
      },
      {
        code: `
          const { Symbol } = primordials;
          const a = { *[Symbol.iterator] () { yield "a"; } }
        `,
        options: [{ name: 'Symbol', ignore: ['iterator'] }]
      },
      {
        code: `
          const { ObjectDefineProperty, Symbol } = primordials;
          ObjectDefineProperty(o, Symbol.toStringTag, { value: "o" })
        `,
        options: [{ name: 'Symbol', ignore: ['toStringTag'] }]
      },
      {
        code: 'const { Symbol } = primordials; Symbol.for("xxx")',
        options: [{ name: 'Symbol', ignore: ['for'] }]
      },
      {
        code: 'const { NumberParseInt } = primordials; NumberParseInt("xxx")',
        options: [{ name: 'parseInt', into: 'Number' }]
      },
      {
        code: `
          const { ReflectOwnKeys } = primordials;
          module.exports = function() {
            ReflectOwnKeys({})
          }
        `,
        options: [{ name: 'Reflect' }],
      },
      {
        code: 'const { Map } = primordials; new Map()',
        options: [{ name: 'Map', into: 'Safe' }],
      },
    ],
    invalid: [
      {
        code: 'new Array()',
        options: [{ name: 'Array' }],
        errors: [{ message: /const { Array } = primordials/ }]
      },
      {
        code: 'JSON.parse("{}")',
        options: [{ name: 'JSON' }],
        errors: [
          { message: /const { JSONParse } = primordials/ },
        ]
      },
      {
        code: 'const { JSON } = primordials; JSON.parse("{}")',
        options: [{ name: 'JSON' }],
        errors: [{ message: /const { JSONParse } = primordials/ }]
      },
      {
        code: 'Symbol.for("xxx")',
        options: [{ name: 'Symbol' }],
        errors: [
          { message: /const { SymbolFor } = primordials/ },
        ]
      },
      {
        code: 'const { Symbol } = primordials; Symbol.for("xxx")',
        options: [{ name: 'Symbol' }],
        errors: [{ message: /const { SymbolFor } = primordials/ }]
      },
      {
        code: `
          const { Symbol } = primordials;
          class A { *[Symbol.iterator] () { yield "a"; } }
        `,
        options: [{ name: 'Symbol' }],
        errors: [{ message: /const { SymbolIterator } = primordials/ }]
      },
      {
        code: `
          const { Symbol } = primordials;
          const a = { *[Symbol.iterator] () { yield "a"; } }
        `,
        options: [{ name: 'Symbol' }],
        errors: [{ message: /const { SymbolIterator } = primordials/ }]
      },
      {
        code: `
          const { ObjectDefineProperty, Symbol } = primordials;
          ObjectDefineProperty(o, Symbol.toStringTag, { value: "o" })
        `,
        options: [{ name: 'Symbol' }],
        errors: [{ message: /const { SymbolToStringTag } = primordials/ }]
      },
      {
        code: `
          const { Number } = primordials;
          Number.parseInt('10')
        `,
        options: [{ name: 'Number' }],
        errors: [{ message: /const { NumberParseInt } = primordials/ }]
      },
      {
        code: 'parseInt("10")',
        options: [{ name: 'parseInt', into: 'Number' }],
        errors: [{ message: /const { NumberParseInt } = primordials/ }]
      },
      {
        code: `
          module.exports = function() {
            const { ownKeys } = Reflect;
          }
        `,
        options: [{ name: 'Reflect' }],
        errors: [{ message: /const { ReflectOwnKeys } = primordials/ }]
      },
      {
        code: 'new Map()',
        options: [{ name: 'Map', into: 'Safe' }],
        errors: [{ message: /const { SafeMap } = primordials/ }]
      },
      {
        code: `
          const { Function } = primordials;
          const noop = Function.prototype;
        `,
        options: [{ name: 'Function' }],
        errors: [{ message: /const { FunctionPrototype } = primordials/ }]
      },
    ]
  });
