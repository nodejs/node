var test = require('tap').test
var v = require('../npm-user-validate.js').pw

test('pw contains a \'', function (t) {
  err = v('\'')
  t.type(err, 'object')
  t.end()
})

test('pw contains a :', function (t) {
  err = v(':')
  t.type(err, 'object')
  t.end()
})

test('pw contains a @', function (t) {
  err = v('@')
  t.type(err, 'object')
  t.end()
})

test('pw contains a "', function (t) {
  err = v('"')
  t.type(err, 'object')
  t.end()
})

test('pw is ok', function (t) {
  err = v('duck')
  t.type(err, 'null')
  t.end()
})