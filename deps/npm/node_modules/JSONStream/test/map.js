
var test = require('tape')

var JSONStream = require('../')

test('map function', function (t) {

  var actual = []

  stream = JSONStream.parse([true], function (e) { return e*10 })
    stream.on('data', function (v) { actual.push(v)})
    stream.on('end', function () {
      t.deepEqual(actual, [10,20,30,40,50,60])
      t.end()

    })

  stream.write(JSON.stringify([1,2,3,4,5,6], null, 2))
  stream.end()

})

test('filter function', function (t) {

  var actual = []

  stream = JSONStream
    .parse([true], function (e) { return e%2 ? e : null})
    .on('data', function (v) { actual.push(v)})
    .on('end', function () {
      t.deepEqual(actual, [1,3,5])
      t.end()

    })

  stream.write(JSON.stringify([1,2,3,4,5,6], null, 2))
  stream.end()

})

