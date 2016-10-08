'use strict'

var rule = require('../rules/catch-or-return')
var RuleTester = require('eslint').RuleTester
var message = 'Expected catch() or return'
var ruleTester = new RuleTester()
ruleTester.run('catch-or-return', rule, {
  valid: [

    // catch
    'frank().then(go).catch(doIt)',
    'frank().then(go).then().then().then().catch(doIt)',
    'frank().then(go).then().catch(function() { /* why bother */ })',
    'frank.then(go).then(to).catch(jail)',
    'Promise.resolve(frank).catch(jail)',
    'frank.then(to).finally(fn).catch(jail)',

    // return
    'function a() { return frank().then(go) }',
    'function a() { return frank().then(go).then().then().then() }',
    'function a() { return frank().then(go).then()}',
    'function a() { return frank.then(go).then(to) }',

    // allowThen - .then(null, fn)
    { code: 'frank().then(go).then(null, doIt)', options: [{ 'allowThen': true }] },
    { code: 'frank().then(go).then().then().then().then(null, doIt)', options: [{ 'allowThen': true }] },
    { code: 'frank().then(go).then().then(null, function() { /* why bother */ })', options: [{ 'allowThen': true }] },
    { code: 'frank.then(go).then(to).then(null, jail)', options: [{ 'allowThen': true }] },

    // allowThen - .then(null, fn)
    { code: 'frank().then(a, b)', options: [{ 'allowThen': true }] },
    { code: 'frank().then(a).then(b).then(null, c)', options: [{ 'allowThen': true }] },
    { code: 'frank().then(a).then(b).then(c, d)', options: [{ 'allowThen': true }] },
    { code: 'frank().then(a).then(b).then().then().then(null, doIt)', options: [{ 'allowThen': true }] },
    { code: 'frank().then(a).then(b).then(null, function() { /* why bother */ })', options: [{ 'allowThen': true }] },
    { code: 'frank.then(go).then(to).then(null, jail)', options: [{ 'allowThen': true }] },

    // allowThen - .then(fn, fn)
    { code: 'frank().then(go).then(zam, doIt)', options: [{ 'allowThen': true }] },
    { code: 'frank().then(go).then().then().then().then(wham, doIt)', options: [{ 'allowThen': true }] },
    { code: 'frank().then(go).then().then(function() {}, function() { /* why bother */ })', options: [{ 'allowThen': true }] },
    { code: 'frank.then(go).then(to).then(pewPew, jail)', options: [{ 'allowThen': true }] },

    // terminationMethod=done - .done(null, fn)
    { code: 'frank().then(go).done()', options: [{ 'terminationMethod': 'done' }] }

  ],

  invalid: [

    // catch failures
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
    },
    {
      code: 'Promise.resolve(frank)',
      errors: [ { message: message } ]
    },
    {
      code: 'frank.then(to).finally(fn)',
      errors: [ { message: message } ]
    },

    // return failures
    { code: 'function a() { frank().then(go) }', errors: [{ message: message }] },
    { code: 'function a() { frank().then(go).then().then().then() }', errors: [{ message: message }] },
    { code: 'function a() { frank().then(go).then()}', errors: [{ message: message }] },
    { code: 'function a() { frank.then(go).then(to) }', errors: [{ message: message }] },

    // terminationMethod=done - .done(null, fn)
    { code: 'frank().then(go)', options: [{ 'terminationMethod': 'done' }], errors: [{ message: 'Expected done() or return' }] },
    { code: 'frank().catch(go)', options: [{ 'terminationMethod': 'done' }], errors: [{ message: 'Expected done() or return' }] }
  ]
})
