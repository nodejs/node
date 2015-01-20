var concat = require('../')
var test = require('tape')

test('no callback stream', function (t) {
  var stream = concat()
  stream.write('space')
  stream.end(' cats')
  t.end()
})

test('no encoding set, no data', function (t) {
  var stream = concat(function(data) {
    t.deepEqual(data, [])
    t.end()
  })
  stream.end()
})

test('encoding set to string, no data', function (t) {
  var stream = concat({ encoding: 'string' }, function(data) {
    t.deepEqual(data, '')
    t.end()
  })
  stream.end()
})
