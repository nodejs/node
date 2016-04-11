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
  var opt = { pathExt: '.sh' }
  var expect = path.resolve(fixture, 'foo.sh').toLowerCase()
  var PATH = process.env.PATH || process.env.Path

  t.test('absolute', function (t) {
    runTest(fixture + '/foo.sh', t)
  })

  t.test('with process.env.PATH', function (t) {
    process.env.PATH = process.env.Path = fixture
    runTest('foo.sh', t)
  })

  t.test('with process.env.Path', {
    skip: isWindows ? false : 'Only for Windows'
  }, function (t) {
    process.env.PATH = ""
    process.env.Path = fixture
    runTest('foo.sh', t)
  })

  t.test('with pathExt', {
    skip: isWindows ? false : 'Only for Windows'
  }, function (t) {
    var pe = process.env.PATHEXT
    process.env.PATHEXT = '.SH'

    t.test('foo.sh', function (t) {
      runTest('foo.sh', t)
    })
    t.test('foo', function (t) {
      runTest('foo', t)
    })
    t.test('replace', function (t) {
      process.env.PATHEXT = pe
      t.end()
    })
    t.end()
  })

  t.test('with path opt', function (t) {
    opt.path = fixture
    runTest('foo.sh', t)
  })

  function runTest(exec, t) {
    t.plan(2)

    var found = which.sync(exec, opt).toLowerCase()
    t.equal(found, expect)

    which(exec, opt, function (er, found) {
      if (er)
        throw er
      t.equal(found.toLowerCase(), expect)
      t.end()
      process.env.PATH = PATH
    })
  }

  t.end()
})

t.test('clean', function (t) {
  rimraf.sync(fixture)
  t.end()
})
