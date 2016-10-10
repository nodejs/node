'use strict'

var rule = require('../rules/no-native')
var RuleTester = require('eslint').RuleTester
var parserOptions = { sourceType: 'module', ecmaVersion: 6 }
var ruleTester = new RuleTester()
ruleTester.run('no-native', rule, {
  valid: [
    'var Promise = null; function x() { return Promise.resolve("hi"); }',
    'var Promise = window.Promise || require("bluebird"); var x = Promise.reject();',
    { code: 'var Promise = null; function x() { return Promise.resolve("hi"); }', parserOptions: parserOptions },
    { code: 'var Promise = window.Promise || require("bluebird"); var x = Promise.reject();', parserOptions: parserOptions },
    { code: 'import Promise from "bluebird"; var x = Promise.reject();', parserOptions: parserOptions }
  ],

  invalid: [
    {
      code: 'new Promise(function(reject, resolve) { })',
      errors: [ { message: '"Promise" is not defined.' } ]
    },
    {
      code: 'Promise.resolve()',
      errors: [ { message: '"Promise" is not defined.' } ]
    },
    {
      code: 'new Promise(function(reject, resolve) { })',
      errors: [ { message: '"Promise" is not defined.' } ],
      env: { browser: true }
    },
    {
      code: 'new Promise(function(reject, resolve) { })',
      errors: [ { message: '"Promise" is not defined.' } ],
      env: { node: true }
    },
    {
      code: 'Promise.resolve()',
      errors: [ { message: '"Promise" is not defined.' } ],
      env: { es6: true }
    },
    {
      code: 'Promise.resolve()',
      errors: [ { message: '"Promise" is not defined.' } ],
      globals: { Promise: true }
    }

  ]
})
