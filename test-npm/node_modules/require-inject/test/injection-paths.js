'use strict'
var path = require('path')
var test = require('tap').test
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir
var requireInject = require('../index')

var testdir = path.join(__dirname, path.basename(__filename, '.js'))
var adir = path.join(testdir, 'a')
var bdir = path.join(testdir, 'b')
var brelative = './' + path.relative(__dirname, bdir)

var fixture = new Tacks(
  Dir({
    'a.js': File(
      "'use strict';\n" +
      "var b = require('./b');\n" +
      'module.exports = function(infile, outfile, cb) {\n' +
      '  b(infile, outfile, cb);\n' +
      '};\n'
    ),
    'b.js': File(
      "'use strict';\n" +
      "var fs = require('fs');\n" +
      'module.exports = function(infile, outfile, cb) {\n' +
      '  fs.rename(infile, outfile, cb)\n' +
      '};\n'
    )
  })
)

test('setup', function (t) {
  fixture.create(testdir)
  t.end()
})

test('mock with absolute path', function (t) {
  t.plan(1)

  var a = requireInject(adir, {
    [bdir]: function (infile, outfile, cb) {
      cb()
    }
  })

  a('in', 'out', function (err) {
    t.notOk(err, 'should be able to rename a file')
  })
})

test('mock with relative path', function (t) {
  t.plan(1)

  var a = requireInject(adir, {
    [brelative]: function (infile, outfile, cb) {
      cb()
    }
  })

  a('in', 'out', function (err) {
    t.notOk(err, 'should be able to rename a file')
  })
})

test('cleanup', function (t) {
  fixture.remove(testdir)
  t.end()
})
