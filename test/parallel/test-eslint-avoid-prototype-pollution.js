'use strict';

const common = require('../common');
if ((!common.hasCrypto) || (!common.hasIntl)) {
  common.skip('ESLint tests require crypto and Intl');
}

common.skipIfEslintMissing();

const RuleTester = require('../../tools/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/avoid-prototype-pollution');

new RuleTester({
  parserOptions: { ecmaVersion: 2022 },
})
  .run('property-descriptor-no-prototype-pollution', rule, {
    valid: [
      'ObjectDefineProperties({}, {})',
      'ObjectCreate(null, {})',
      'ObjectDefineProperties({}, { key })',
      'ObjectCreate(null, { key })',
      'ObjectDefineProperties({}, { ...spread })',
      'ObjectCreate(null, { ...spread })',
      'ObjectDefineProperties({}, { key: valueDescriptor })',
      'ObjectCreate(null, { key: valueDescriptor })',
      'ObjectDefineProperties({}, { key: { ...{}, __proto__: null } })',
      'ObjectCreate(null, { key: { ...{}, __proto__: null } })',
      'ObjectDefineProperties({}, { key: { __proto__: null } })',
      'ObjectCreate(null, { key: { __proto__: null } })',
      'ObjectDefineProperties({}, { key: { __proto__: null, enumerable: true } })',
      'ObjectCreate(null, { key: { __proto__: null, enumerable: true } })',
      'ObjectDefineProperties({}, { key: { "__proto__": null } })',
      'ObjectCreate(null, { key: { "__proto__": null } })',
      'ObjectDefineProperties({}, { key: { \'__proto__\': null } })',
      'ObjectCreate(null, { key: { \'__proto__\': null } })',
      'ObjectDefineProperty({}, "key", ObjectCreate(null))',
      'ReflectDefineProperty({}, "key", ObjectCreate(null))',
      'ObjectDefineProperty({}, "key", valueDescriptor)',
      'ReflectDefineProperty({}, "key", valueDescriptor)',
      'ObjectDefineProperty({}, "key", { __proto__: null })',
      'ReflectDefineProperty({}, "key", { __proto__: null })',
      'ObjectDefineProperty({}, "key", { __proto__: null, enumerable: true })',
      'ReflectDefineProperty({}, "key", { __proto__: null, enumerable: true })',
      'ObjectDefineProperty({}, "key", { "__proto__": null })',
      'ReflectDefineProperty({}, "key", { "__proto__": null })',
      'ObjectDefineProperty({}, "key", { \'__proto__\': null })',
      'ReflectDefineProperty({}, "key", { \'__proto__\': null })',
    ],
    invalid: [
      {
        code: 'ObjectDefineProperties({}, ObjectGetOwnPropertyDescriptors({}))',
        errors: [{ message: /prototype pollution/ }],
      },
      {
        code: 'ObjectCreate(null, ObjectGetOwnPropertyDescriptors({}))',
        errors: [{ message: /prototype pollution/ }],
      },
      {
        code: 'ObjectDefineProperties({}, { key: {} })',
        errors: [{ message: /null-prototype/ }],
      },
      {
        code: 'ObjectCreate(null, { key: {} })',
        errors: [{ message: /null-prototype/ }],
      },
      {
        code: 'ObjectDefineProperties({}, { key: { [void 0]: { ...{ __proto__: null } } } })',
        errors: [{ message: /null-prototype/ }],
      },
      {
        code: 'ObjectCreate(null, { key: { [void 0]: { ...{ __proto__: null } } } })',
        errors: [{ message: /null-prototype/ }],
      },
      {
        code: 'ObjectDefineProperties({}, { key: { __proto__: Object.prototype } })',
        errors: [{ message: /null-prototype/ }],
      },
      {
        code: 'ObjectCreate(null, { key: { __proto__: Object.prototype } })',
        errors: [{ message: /null-prototype/ }],
      },
      {
        code: 'ObjectDefineProperties({}, { key: { [`__proto__`]: null } })',
        errors: [{ message: /null-prototype/ }],
      },
      {
        code: 'ObjectCreate(null, { key: { [`__proto__`]: null } })',
        errors: [{ message: /null-prototype/ }],
      },
      {
        code: 'ObjectDefineProperties({}, { key: { enumerable: true } })',
        errors: [{ message: /null-prototype/ }],
      },
      {
        code: 'ObjectCreate(null, { key: { enumerable: true } })',
        errors: [{ message: /null-prototype/ }],
      },
      {
        code: 'ObjectDefineProperty({}, "key", {})',
        errors: [{ message: /null-prototype/ }],
      },
      {
        code: 'ReflectDefineProperty({}, "key", {})',
        errors: [{ message: /null-prototype/ }],
      },
      {
        code: 'ObjectDefineProperty({}, "key", ObjectGetOwnPropertyDescriptor({}, "key"))',
        errors: [{ message: /prototype pollution/ }],
      },
      {
        code: 'ReflectDefineProperty({}, "key", ObjectGetOwnPropertyDescriptor({}, "key"))',
        errors: [{ message: /prototype pollution/ }],
      },
      {
        code: 'ObjectDefineProperty({}, "key", ReflectGetOwnPropertyDescriptor({}, "key"))',
        errors: [{ message: /prototype pollution/ }],
      },
      {
        code: 'ReflectDefineProperty({}, "key", ReflectGetOwnPropertyDescriptor({}, "key"))',
        errors: [{ message: /prototype pollution/ }],
      },
      {
        code: 'ObjectDefineProperty({}, "key", { __proto__: Object.prototype })',
        errors: [{ message: /null-prototype/ }],
      },
      {
        code: 'ReflectDefineProperty({}, "key", { __proto__: Object.prototype })',
        errors: [{ message: /null-prototype/ }],
      },
      {
        code: 'ObjectDefineProperty({}, "key", { [`__proto__`]: null })',
        errors: [{ message: /null-prototype/ }],
      },
      {
        code: 'ReflectDefineProperty({}, "key", { [`__proto__`]: null })',
        errors: [{ message: /null-prototype/ }],
      },
      {
        code: 'ObjectDefineProperty({}, "key", { enumerable: true })',
        errors: [{ message: /null-prototype/ }],
      },
      {
        code: 'ReflectDefineProperty({}, "key", { enumerable: true })',
        errors: [{ message: /null-prototype/ }],
      },
    ]
  });
