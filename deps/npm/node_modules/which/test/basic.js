var t = require('tap')
var fs = require('fs')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var fixture = __dirname + '/fixture'
var which = require('../which.js')
var path = require('path')

var isWindows = process.platform === 'win32' ||
    process.env.OSTYPE === 'cygwin' ||
    process.env.OSTYPE === 'msys'

var skip = { skip: isWindows ? 'not relevant on windows' : false }

t.test('setup', function (t) {
  rimraf.sync(fixture)
  mkdirp.sync(fixture)
  fs.writeFileSync(fixture + '/foo.sh', 'echo foo\n')
  t.end()
})

t.test('does not find non-executable', skip, function (t) {
  t.plan(2)

  t.test('absolute', function (t) {
    t.plan(2)
    which(fixture + '/foo.sh', function (er) {
      t.isa(er, Error)
    })

    t.throws(function () {
      which.sync(fixture + '/foo.sh')
    })
  })

  t.test('with path', function (t) {
    t.plan(2)
    which('foo.sh', { path: fixture }, function (er) {
      t.isa(er, Error)
    })

    t.throws(function () {
      which.sync('foo.sh', { path: fixture })
    })
  })
})

t.test('make executable', function (t) {
  fs.chmodSync(fixture + '/foo.sh', '0755')
  t.end()
})

t.test('find when executable', function (t) {
  t.plan(2)
  var opt = { pathExt: '.sh' }
  var expect = path.resolve(fixture, 'foo.sh').toLowerCase()

  t.test('absolute', function (t) {
    t.plan(2)
    runTest(t)
  })

  function runTest(t) {
    which(fixture + '/foo.sh', opt, function (er, found) {
      if (er)
        throw er
      t.equal(found.toLowerCase(), expect)
    })

    var found = which.sync(fixture + '/foo.sh', opt).toLowerCase()
    t.equal(found, expect)
  }

  t.test('with path', function (t) {
    t.plan(2)
    opt.path = fixture
    runTest(t)
  })
})

t.test('clean', function (t) {
  rimraf.sync(fixture)
  t.end()
})
