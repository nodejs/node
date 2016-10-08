'use strict'
var test = require('tap').test
var requireInject = require('../index')
var path = require('path')
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir

var fixturepath = path.resolve(__dirname, 'lib')

var fixture = new Tacks(
  Dir({
    'a.js': File(
      "'use strict'\n" +
      "var b = require('./b')\n" +
      'module.exports = function(infile, outfile, cb) {\n' +
      '  b(infile, outfile, cb)\n' +
      '}\n'
    ),
    'b.js': File(
      "'use strict'\n" +
      "var fs = require('fs')\n" +
      'module.exports = function(infile, outfile, cb) {\n' +
      '  fs.rename(infile, outfile, cb)\n' +
      '}\n'
    ),
    'd.js': File(
      "'use strict'\n" +
      "var b = require('./b')\n" +
      "var e = require('./e')\n" +
      'module.exports.b = function(infile, outfile, cb) {\n' +
      '  b(infile, outfile, cb)\n' +
      '}\n' +
      'module.exports.e = function(infile, outfile, cb) {\n' +
      '  e(infile, outfile, cb)\n' +
      '}\n'
    ),
    'e.js': File(
      "'use strict'\n" +
      "var fs = require('fs')\n" +
      'module.exports = function(infile, outfile, cb) {\n' +
      '  fs.rename(infile, outfile, cb)\n' +
      '}\n'
    )
  })
)

test('setup', function (t) {
  setup()
  t.done()
})

test('injection leaking at a distance', function (t) {
  t.plan(2)
  var first = require('./lib/a')

  first('in', 'out', function (err) { t.ok(err, "shouldn't be able to rename") })

  var second = requireInject.withEmptyCache('./lib/a', {
    'fs': {
      rename: function (infile, outfile, cb) {
        cb()
      }
    }
  })

  second('in', 'out', function (err) { t.notOk(err, 'should be able to rename a file now') })
})
test('cleanup', function (t) {
  cleanup()
  t.done()
})

function setup () {
  cleanup()
  fixture.create(fixturepath)
}
function cleanup () {
  fixture.remove(fixturepath)
}
