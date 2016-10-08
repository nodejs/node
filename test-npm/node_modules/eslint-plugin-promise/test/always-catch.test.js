'use strict'

var rule = require('../rules/always-catch')
var RuleTester = require('eslint').RuleTester
var message = 'You should always catch() a then()'
var ruleTester = new RuleTester()
ruleTester.run('always-catch', rule, {
  valid: [
    'frank().then(go).catch(doIt)',
    'frank().then(go).then().then().then().catch(doIt)',
    'frank().then(go).then().catch(function() { /* why bother */ })',
    'frank.then(go).then(to).catch(jail)'
  ],

  invalid: [
    {
      code: 'function callPromise(promise, cb) { promise.then(cb) }',
      errors: [ { message: message } ]
    },
    {
      code: 'fetch("http://www.yahoo.com").then(console.log.bind(console))',
      errors: [ { message: message } ]
    },
    {
      code: 'a.then(function() { return "x"; }).then(function(y) { throw y; })',
      errors: [ { message: message } ]
    },
    {
      code: 'a.then(function() { return "x"; }).then(function(y) { throw y; })',
      errors: [ { message: message } ]
    }

  ]
})
