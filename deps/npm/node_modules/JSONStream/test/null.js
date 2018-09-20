var JSONStream = require('../')

var data = [
  {ID: 1, optional: null},
  {ID: 2, optional: null},
  {ID: 3, optional: 20},
  {ID: 4, optional: null},
  {ID: 5, optional: 'hello'},
  {ID: 6, optional: null}
]


var test = require('tape')

test ('null properties', function (t) {
  var actual = []
  var stream =

  JSONStream.parse('*.optional')
    .on('data', function (v) { actual.push(v) })
    .on('end', function () {
      t.deepEqual(actual, [20, 'hello'])
      t.end()
    })

  stream.write(JSON.stringify(data, null, 2))
  stream.end()
})
