var test = require('tap').test
var v = require('../npm-user-validate.js').email

test('email misses an @', function (t) {
  err = v('namedomain')
  t.type(err, 'object')
  t.end()
})

test('email misses a dot', function (t) {
  err = v('name@domain')
  t.type(err, 'object')
  t.end()
})

test('email misses a string before the @', function (t) {
  err = v('@domain')
  t.type(err, 'object')
  t.end()
})

test('email is ok', function (t) {
  err = v('name@domain.com')
  t.type(err, 'null')
  t.end()
})