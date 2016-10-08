'use strict'

var rule = require('../rules/param-names')
var RuleTester = require('eslint').RuleTester

var ruleTester = new RuleTester()
ruleTester.run('param-names', rule, {
  valid: [
    'new Promise(function(resolve, reject) { })',
    'new Promise(function(resolve) { })'
  ],

  invalid: [
    {
      code: 'new Promise(function(reject, resolve) { })',
      errors: [ { message: 'Promise constructor parameters must be named resolve, reject' } ]
    },
    {
      code: 'new Promise(function(resolve, rej) { })',
      errors: [ { message: 'Promise constructor parameters must be named resolve, reject' } ]
    }
  ]
})
