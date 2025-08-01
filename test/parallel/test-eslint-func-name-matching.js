// @fileoverview Tests for func-name-matching rule.
// @author Annie Zhang

'use strict';
const common = require('../common');
if ((!common.hasCrypto) || (!common.hasIntl)) {
  common.skip('ESLint tests require crypto and Intl');
}
common.skipIfEslintMissing();

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

const rule = require('../../tools/eslint-rules/func-name-matching');
const RuleTester = require('../../tools/eslint/node_modules/eslint').RuleTester;

//------------------------------------------------------------------------------
// Tests
//------------------------------------------------------------------------------

const ruleTester = new RuleTester();

ruleTester.run('func-name-matching', rule, {
  valid: [
    'var foo;',
    'var foo = function foo() {};',
    { code: 'var foo = function foo() {};', options: ['always'] },
    { code: 'var foo = function bar() {};', options: ['never'] },
    'var foo = function() {}',
    { code: 'var foo = () => {}', languageOptions: { ecmaVersion: 6 } },
    'foo = function foo() {};',
    { code: 'foo = function foo() {};', options: ['always'] },
    { code: 'foo = function bar() {};', options: ['never'] },
    {
      code: 'foo &&= function foo() {};',
      languageOptions: { ecmaVersion: 2021 },
    },
    {
      code: 'obj.foo ||= function foo() {};',
      languageOptions: { ecmaVersion: 2021 },
    },
    {
      code: 'obj[\'foo\'] ??= function foo() {};',
      languageOptions: { ecmaVersion: 2021 },
    },
    'obj.foo = function foo() {};',
    { code: 'obj.foo = function foo() {};', options: ['always'] },
    { code: 'obj.foo = function bar() {};', options: ['never'] },
    'obj.foo = function() {};',
    { code: 'obj.foo = function() {};', options: ['always'] },
    { code: 'obj.foo = function() {};', options: ['never'] },
    'obj.bar.foo = function foo() {};',
    { code: 'obj.bar.foo = function foo() {};', options: ['always'] },
    { code: 'obj.bar.foo = function baz() {};', options: ['never'] },
    'obj[\'foo\'] = function foo() {};',
    { code: 'obj[\'foo\'] = function foo() {};', options: ['always'] },
    { code: 'obj[\'foo\'] = function bar() {};', options: ['never'] },
    'obj[\'foo//bar\'] = function foo() {};',
    { code: 'obj[\'foo//bar\'] = function foo() {};', options: ['always'] },
    { code: 'obj[\'foo//bar\'] = function foo() {};', options: ['never'] },
    'obj[foo] = function bar() {};',
    { code: 'obj[foo] = function bar() {};', options: ['always'] },
    { code: 'obj[foo] = function bar() {};', options: ['never'] },
    'var obj = {foo: function foo() {}};',
    { code: 'var obj = {foo: function foo() {}};', options: ['always'] },
    { code: 'var obj = {foo: function bar() {}};', options: ['never'] },
    'var obj = {\'foo\': function foo() {}};',
    { code: 'var obj = {\'foo\': function foo() {}};', options: ['always'] },
    { code: 'var obj = {\'foo\': function bar() {}};', options: ['never'] },
    'var obj = {\'foo//bar\': function foo() {}};',
    {
      code: 'var obj = {\'foo//bar\': function foo() {}};',
      options: ['always'],
    },
    {
      code: 'var obj = {\'foo//bar\': function foo() {}};',
      options: ['never'],
    },
    'var obj = {foo: function() {}};',
    { code: 'var obj = {foo: function() {}};', options: ['always'] },
    { code: 'var obj = {foo: function() {}};', options: ['never'] },
    {
      code: 'var obj = {[foo]: function bar() {}} ',
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: 'var obj = {[\'x\' + 2]: function bar(){}};',
      languageOptions: { ecmaVersion: 6 },
    },
    'obj[\'x\' + 2] = function bar(){};',
    {
      code: 'var [ bar ] = [ function bar(){} ];',
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: 'function a(foo = function bar() {}) {}',
      languageOptions: { ecmaVersion: 6 },
    },
    'module.exports = function foo(name) {};',
    'module[\'exports\'] = function foo(name) {};',
    {
      code: 'module.exports = function foo(name) {};',
      options: [{ includeCommonJSModuleExports: false }],
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: 'module.exports = function foo(name) {};',
      options: ['always', { includeCommonJSModuleExports: false }],
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: 'module.exports = function foo(name) {};',
      options: ['never', { includeCommonJSModuleExports: false }],
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: 'module[\'exports\'] = function foo(name) {};',
      options: [{ includeCommonJSModuleExports: false }],
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: 'module[\'exports\'] = function foo(name) {};',
      options: ['always', { includeCommonJSModuleExports: false }],
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: 'module[\'exports\'] = function foo(name) {};',
      options: ['never', { includeCommonJSModuleExports: false }],
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: '({[\'foo\']: function foo() {}})',
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: '({[\'foo\']: function foo() {}})',
      options: ['always'],
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: '({[\'foo\']: function bar() {}})',
      options: ['never'],
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: '({[\'❤\']: function foo() {}})',
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: '({[foo]: function bar() {}})',
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: '({[null]: function foo() {}})',
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: '({[1]: function foo() {}})',
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: '({[true]: function foo() {}})',
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: '({[`x`]: function foo() {}})',
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: '({[/abc/]: function foo() {}})',
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: '({[[1, 2, 3]]: function foo() {}})',
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: '({[{x: 1}]: function foo() {}})',
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: '[] = function foo() {}',
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: '({} = function foo() {})',
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: '[a] = function foo() {}',
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: '({a} = function foo() {})',
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: 'var [] = function foo() {}',
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: 'var {} = function foo() {}',
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: 'var [a] = function foo() {}',
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: 'var {a} = function foo() {}',
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: '({ value: function value() {} })',
      options: [{ considerPropertyDescriptor: true }],
    },
    {
      code: 'obj.foo = function foo() {};',
      options: ['always', { considerPropertyDescriptor: true }],
    },
    {
      code: 'obj.bar.foo = function foo() {};',
      options: ['always', { considerPropertyDescriptor: true }],
    },
    {
      code: 'var obj = {foo: function foo() {}};',
      options: ['always', { considerPropertyDescriptor: true }],
    },
    {
      code: 'var obj = {foo: function() {}};',
      options: ['always', { considerPropertyDescriptor: true }],
    },
    {
      code: 'var obj = { value: function value() {} }',
      options: ['always', { considerPropertyDescriptor: true }],
    },
    {
      code: 'Object.defineProperty(foo, \'bar\', { value: function bar() {} })',
      options: ['always', { considerPropertyDescriptor: true }],
    },
    {
      code: 'Object.defineProperties(foo, { bar: { value: function bar() {} } })',
      options: ['always', { considerPropertyDescriptor: true }],
    },
    {
      code: 'Object.create(proto, { bar: { value: function bar() {} } })',
      options: ['always', { considerPropertyDescriptor: true }],
    },
    {
      code: 'Object.defineProperty(foo, \'b\' + \'ar\', { value: function bar() {} })',
      options: ['always', { considerPropertyDescriptor: true }],
    },
    {
      code: 'Object.defineProperties(foo, { [\'bar\']: { value: function bar() {} } })',
      options: ['always', { considerPropertyDescriptor: true }],
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: 'Object.create(proto, { [\'bar\']: { value: function bar() {} } })',
      options: ['always', { considerPropertyDescriptor: true }],
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: 'Object.defineProperty(foo, \'bar\', { value() {} })',
      options: ['never', { considerPropertyDescriptor: true }],
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: 'Object.defineProperties(foo, { bar: { value() {} } })',
      options: ['never', { considerPropertyDescriptor: true }],
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: 'Object.create(proto, { bar: { value() {} } })',
      options: ['never', { considerPropertyDescriptor: true }],
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: 'Reflect.defineProperty(foo, \'bar\', { value: function bar() {} })',
      options: ['always', { considerPropertyDescriptor: true }],
    },
    {
      code: 'Reflect.defineProperty(foo, \'b\' + \'ar\', { value: function baz() {} })',
      options: ['always', { considerPropertyDescriptor: true }],
    },
    {
      code: 'Reflect.defineProperty(foo, \'bar\', { value() {} })',
      options: ['never', { considerPropertyDescriptor: true }],
      languageOptions: { ecmaVersion: 6 },
    },
    {
      code: 'foo({ value: function value() {} })',
      options: ['always', { considerPropertyDescriptor: true }],
    },

    // Class fields, private names are ignored
    {
      code: 'class C { x = function () {}; }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { x = function () {}; }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { \'x\' = function () {}; }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { \'x\' = function () {}; }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { #x = function () {}; }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { #x = function () {}; }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { [x] = function () {}; }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { [x] = function () {}; }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { [\'x\'] = function () {}; }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { [\'x\'] = function () {}; }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { x = function x() {}; }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { x = function y() {}; }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { \'x\' = function x() {}; }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { \'x\' = function y() {}; }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { #x = function x() {}; }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { #x = function x() {}; }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { #x = function y() {}; }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { #x = function y() {}; }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { [x] = function x() {}; }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { [x] = function x() {}; }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { [x] = function y() {}; }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { [x] = function y() {}; }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { [\'x\'] = function x() {}; }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { [\'x\'] = function y() {}; }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { \'xy \' = function foo() {}; }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { \'xy \' = function xy() {}; }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { [\'xy \'] = function foo() {}; }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { [\'xy \'] = function xy() {}; }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { 1 = function x0() {}; }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { 1 = function x1() {}; }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { [1] = function x0() {}; }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { [1] = function x1() {}; }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { [f()] = function g() {}; }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { [f()] = function f() {}; }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { static x = function x() {}; }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { static x = function y() {}; }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { x = (function y() {})(); }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { x = (function x() {})(); }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: '(class { x = function x() {}; })',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: '(class { x = function y() {}; })',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { #x; foo() { this.#x = function x() {}; } }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { #x; foo() { this.#x = function x() {}; } }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { #x; foo() { this.#x = function y() {}; } }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { #x; foo() { this.#x = function y() {}; } }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { #x; foo() { a.b.#x = function x() {}; } }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { #x; foo() { a.b.#x = function x() {}; } }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { #x; foo() { a.b.#x = function y() {}; } }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'class C { #x; foo() { a.b.#x = function y() {}; } }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
    },
    {
      code: 'var obj = { \'\\u1885\': function foo() {} };', // Not a valid identifier in es5
      languageOptions: {
        ecmaVersion: 5,
        sourceType: 'script',
      },
    },
  ],
  invalid: [
    {
      code: 'let foo = function bar() {};',
      options: ['always'],
      languageOptions: { ecmaVersion: 6 },
      errors: [
        {
          messageId: 'matchVariable',
          data: { funcName: 'bar', name: 'foo' },
        },
      ],
    },
    {
      code: 'let foo = function bar() {};',
      languageOptions: { ecmaVersion: 6 },
      errors: [
        {
          messageId: 'matchVariable',
          data: { funcName: 'bar', name: 'foo' },
        },
      ],
    },
    {
      code: 'foo = function bar() {};',
      languageOptions: { ecmaVersion: 6 },
      errors: [
        {
          messageId: 'matchVariable',
          data: { funcName: 'bar', name: 'foo' },
        },
      ],
    },
    {
      code: 'foo &&= function bar() {};',
      languageOptions: { ecmaVersion: 2021 },
      errors: [
        {
          messageId: 'matchVariable',
          data: { funcName: 'bar', name: 'foo' },
        },
      ],
    },
    {
      code: 'obj.foo ||= function bar() {};',
      languageOptions: { ecmaVersion: 2021 },
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'bar', name: 'foo' },
        },
      ],
    },
    {
      code: 'obj[\'foo\'] ??= function bar() {};',
      languageOptions: { ecmaVersion: 2021 },
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'bar', name: 'foo' },
        },
      ],
    },
    {
      code: 'obj.foo = function bar() {};',
      languageOptions: { ecmaVersion: 6 },
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'bar', name: 'foo' },
        },
      ],
    },
    {
      code: 'obj.bar.foo = function bar() {};',
      languageOptions: { ecmaVersion: 6 },
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'bar', name: 'foo' },
        },
      ],
    },
    {
      code: 'obj[\'foo\'] = function bar() {};',
      languageOptions: { ecmaVersion: 6 },
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'bar', name: 'foo' },
        },
      ],
    },
    {
      code: 'let obj = {foo: function bar() {}};',
      languageOptions: { ecmaVersion: 6 },
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'bar', name: 'foo' },
        },
      ],
    },
    {
      code: 'let obj = {\'foo\': function bar() {}};',
      languageOptions: { ecmaVersion: 6 },
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'bar', name: 'foo' },
        },
      ],
    },
    {
      code: '({[\'foo\']: function bar() {}})',
      languageOptions: { ecmaVersion: 6 },
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'bar', name: 'foo' },
        },
      ],
    },
    {
      code: 'module.exports = function foo(name) {};',
      options: [{ includeCommonJSModuleExports: true }],
      languageOptions: { ecmaVersion: 6 },
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'foo', name: 'exports' },
        },
      ],
    },
    {
      code: 'module.exports = function foo(name) {};',
      options: ['always', { includeCommonJSModuleExports: true }],
      languageOptions: { ecmaVersion: 6 },
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'foo', name: 'exports' },
        },
      ],
    },
    {
      code: 'module.exports = function exports(name) {};',
      options: ['never', { includeCommonJSModuleExports: true }],
      languageOptions: { ecmaVersion: 6 },
      errors: [
        {
          messageId: 'notMatchProperty',
          data: { funcName: 'exports', name: 'exports' },
        },
      ],
    },
    {
      code: 'module[\'exports\'] = function foo(name) {};',
      options: [{ includeCommonJSModuleExports: true }],
      languageOptions: { ecmaVersion: 6 },
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'foo', name: 'exports' },
        },
      ],
    },
    {
      code: 'module[\'exports\'] = function foo(name) {};',
      options: ['always', { includeCommonJSModuleExports: true }],
      languageOptions: { ecmaVersion: 6 },
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'foo', name: 'exports' },
        },
      ],
    },
    {
      code: 'module[\'exports\'] = function exports(name) {};',
      options: ['never', { includeCommonJSModuleExports: true }],
      languageOptions: { ecmaVersion: 6 },
      errors: [
        {
          messageId: 'notMatchProperty',
          data: { funcName: 'exports', name: 'exports' },
        },
      ],
    },
    {
      code: 'var foo = function foo(name) {};',
      options: ['never'],
      errors: [
        {
          messageId: 'notMatchVariable',
          data: { funcName: 'foo', name: 'foo' },
        },
      ],
    },
    {
      code: 'obj.foo = function foo(name) {};',
      options: ['never'],
      errors: [
        {
          messageId: 'notMatchProperty',
          data: { funcName: 'foo', name: 'foo' },
        },
      ],
    },
    {
      code: 'Object.defineProperty(foo, \'bar\', { value: function baz() {} })',
      options: ['always', { considerPropertyDescriptor: true }],
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'baz', name: 'bar' },
        },
      ],
    },
    {
      code: 'Object.defineProperties(foo, { bar: { value: function baz() {} } })',
      options: ['always', { considerPropertyDescriptor: true }],
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'baz', name: 'bar' },
        },
      ],
    },
    {
      code: 'Object.create(proto, { bar: { value: function baz() {} } })',
      options: ['always', { considerPropertyDescriptor: true }],
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'baz', name: 'bar' },
        },
      ],
    },
    {
      code: 'var obj = { value: function foo(name) {} }',
      options: ['always', { considerPropertyDescriptor: true }],
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'foo', name: 'value' },
        },
      ],
    },
    {
      code: 'Object.defineProperty(foo, \'bar\', { value: function bar() {} })',
      options: ['never', { considerPropertyDescriptor: true }],
      errors: [
        {
          messageId: 'notMatchProperty',
          data: { funcName: 'bar', name: 'bar' },
        },
      ],
    },
    {
      code: 'Object.defineProperties(foo, { bar: { value: function bar() {} } })',
      options: ['never', { considerPropertyDescriptor: true }],
      errors: [
        {
          messageId: 'notMatchProperty',
          data: { funcName: 'bar', name: 'bar' },
        },
      ],
    },
    {
      code: 'Object.create(proto, { bar: { value: function bar() {} } })',
      options: ['never', { considerPropertyDescriptor: true }],
      errors: [
        {
          messageId: 'notMatchProperty',
          data: { funcName: 'bar', name: 'bar' },
        },
      ],
    },
    {
      code: 'Reflect.defineProperty(foo, \'bar\', { value: function baz() {} })',
      options: ['always', { considerPropertyDescriptor: true }],
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'baz', name: 'bar' },
        },
      ],
    },
    {
      code: 'Reflect.defineProperty(foo, \'bar\', { value: function bar() {} })',
      options: ['never', { considerPropertyDescriptor: true }],
      errors: [
        {
          messageId: 'notMatchProperty',
          data: { funcName: 'bar', name: 'bar' },
        },
      ],
    },
    // Tests for Node's primordials (ObjectDefineProperty, ObjectDefineProperties, ReflectDefineProperty)
    {
      code: 'ObjectDefineProperty(foo, \'bar\', { value: function baz() {} })',
      options: ['always', { considerPropertyDescriptor: true }],
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'baz', name: 'bar' },
        },
      ],
    },
    {
      code: 'ReflectDefineProperty(foo, \'bar\', { value: function baz() {} })',
      options: ['always', { considerPropertyDescriptor: true }],
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'baz', name: 'bar' },
        },
      ],
    },
    {
      code: 'ObjectDefineProperties(foo, { bar: { value: function baz() {} } })',
      options: ['always', { considerPropertyDescriptor: true }],
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'baz', name: 'bar' },
        },
      ],
    },
    {
      code: 'foo({ value: function bar() {} })',
      options: ['always', { considerPropertyDescriptor: true }],
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'bar', name: 'value' },
        },
      ],
    },

    // Optional chaining
    {
      code: '(obj?.aaa).foo = function bar() {};',
      languageOptions: { ecmaVersion: 2020 },
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'bar', name: 'foo' },
        },
      ],
    },
    {
      code: 'Object?.defineProperty(foo, \'bar\', { value: function baz() {} })',
      options: ['always', { considerPropertyDescriptor: true }],
      languageOptions: { ecmaVersion: 2020 },
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'baz', name: 'bar' },
        },
      ],
    },
    {
      code: '(Object?.defineProperty)(foo, \'bar\', { value: function baz() {} })',
      options: ['always', { considerPropertyDescriptor: true }],
      languageOptions: { ecmaVersion: 2020 },
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'baz', name: 'bar' },
        },
      ],
    },
    {
      code: 'Object?.defineProperty(foo, \'bar\', { value: function bar() {} })',
      options: ['never', { considerPropertyDescriptor: true }],
      languageOptions: { ecmaVersion: 2020 },
      errors: [
        {
          messageId: 'notMatchProperty',
          data: { funcName: 'bar', name: 'bar' },
        },
      ],
    },
    {
      code: '(Object?.defineProperty)(foo, \'bar\', { value: function bar() {} })',
      options: ['never', { considerPropertyDescriptor: true }],
      languageOptions: { ecmaVersion: 2020 },
      errors: [
        {
          messageId: 'notMatchProperty',
          data: { funcName: 'bar', name: 'bar' },
        },
      ],
    },
    {
      code: 'Object?.defineProperties(foo, { bar: { value: function baz() {} } })',
      options: ['always', { considerPropertyDescriptor: true }],
      languageOptions: { ecmaVersion: 2020 },
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'baz', name: 'bar' },
        },
      ],
    },
    {
      code: '(Object?.defineProperties)(foo, { bar: { value: function baz() {} } })',
      options: ['always', { considerPropertyDescriptor: true }],
      languageOptions: { ecmaVersion: 2020 },
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'baz', name: 'bar' },
        },
      ],
    },
    {
      code: 'Object?.defineProperties(foo, { bar: { value: function bar() {} } })',
      options: ['never', { considerPropertyDescriptor: true }],
      languageOptions: { ecmaVersion: 2020 },
      errors: [
        {
          messageId: 'notMatchProperty',
          data: { funcName: 'bar', name: 'bar' },
        },
      ],
    },
    {
      code: '(Object?.defineProperties)(foo, { bar: { value: function bar() {} } })',
      options: ['never', { considerPropertyDescriptor: true }],
      languageOptions: { ecmaVersion: 2020 },
      errors: [
        {
          messageId: 'notMatchProperty',
          data: { funcName: 'bar', name: 'bar' },
        },
      ],
    },

    // class fields
    {
      code: 'class C { x = function y() {}; }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'y', name: 'x' },
        },
      ],
    },
    {
      code: 'class C { x = function x() {}; }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
      errors: [
        {
          messageId: 'notMatchProperty',
          data: { funcName: 'x', name: 'x' },
        },
      ],
    },
    {
      code: 'class C { \'x\' = function y() {}; }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'y', name: 'x' },
        },
      ],
    },
    {
      code: 'class C { \'x\' = function x() {}; }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
      errors: [
        {
          messageId: 'notMatchProperty',
          data: { funcName: 'x', name: 'x' },
        },
      ],
    },
    {
      code: 'class C { [\'x\'] = function y() {}; }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'y', name: 'x' },
        },
      ],
    },
    {
      code: 'class C { [\'x\'] = function x() {}; }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
      errors: [
        {
          messageId: 'notMatchProperty',
          data: { funcName: 'x', name: 'x' },
        },
      ],
    },
    {
      code: 'class C { static x = function y() {}; }',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'y', name: 'x' },
        },
      ],
    },
    {
      code: 'class C { static x = function x() {}; }',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
      errors: [
        {
          messageId: 'notMatchProperty',
          data: { funcName: 'x', name: 'x' },
        },
      ],
    },
    {
      code: '(class { x = function y() {}; })',
      options: ['always'],
      languageOptions: { ecmaVersion: 2022 },
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'y', name: 'x' },
        },
      ],
    },
    {
      code: '(class { x = function x() {}; })',
      options: ['never'],
      languageOptions: { ecmaVersion: 2022 },
      errors: [
        {
          messageId: 'notMatchProperty',
          data: { funcName: 'x', name: 'x' },
        },
      ],
    },
    {
      code: 'var obj = { \'\\u1885\': function foo() {} };', // Valid identifier in es2015
      languageOptions: { ecmaVersion: 6 },
      errors: [
        {
          messageId: 'matchProperty',
          data: { funcName: 'foo', name: '\u1885' },
        },
      ],
    },
  ],
});
