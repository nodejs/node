var test = require('tap').test
var v = require('../npm-user-validate.js').username

test('username must be lowercase', function (t) {
  err = v('ERRR')
  t.type(err, 'object')
  t.end()
})

test('username may not contain non-url-safe chars', function (t) {
  err = v('f  ')
  t.type(err, 'object')
  t.end()
})

test('username may not start with "."', function (t) {
  err = v('.username')
  t.type(err, 'object')
  t.end()
})

test('username is ok', function (t) {
  err = v('ente')
  t.type(err, 'null')
  t.end()
})
