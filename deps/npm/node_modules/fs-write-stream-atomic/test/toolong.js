var path = require('path')
var test = require('tap').test
var writeStream = require('../index.js')

function repeat(times, string) {
  var output = ''
  for (var ii = 0; ii < times; ++ii) {
    output += string
  }
  return output
}

var target = path.resolve(__dirname, repeat(1000,'test'))

test('name too long', function (t) {
  var stream = writeStream(target)
  stream.on('error', function (er) {
    t.is(er.code, 'ENAMETOOLONG', target.length + " character name results in ENAMETOOLONG")
  })
  stream.on('close', function () {
    t.end()
  })
  stream.end()
})