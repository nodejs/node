'use strict'
var test = require('tap').test
var path = require('path')
var rimraf = require('rimraf')
var common = require('../common-tap.js')
var basepath = common.pkg
var fixturepath = path.resolve(basepath, 'npm-test-files')
var pkgpath = path.resolve(fixturepath, 'npm-test-ls')
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir

test('ls without arg', function (t) {
  var fixture = new Tacks(
    Dir({
      'npm-test-ls': Dir({
        'package.json': File({
          name: 'npm-test-ls',
          version: '1.0.0',
          dependencies: {
            'dep': 'file:../dep'
          }
        })
      }),
      'dep': Dir({
        'package.json': File({
          name: 'dep',
          version: '1.0.0'
        })
      })
    })
  )
  withFixture(t, fixture, function (done) {
    common.npm([
      'ls', '--json'
    ], {
      cwd: pkgpath
    }, function (err, code, stdout, stderr) {
      t.ifErr(err, 'ls succeeded')
      t.equal(0, code, 'exit 0 on ls')
      var pkg = JSON.parse(stdout)
      var deps = pkg.dependencies
      t.ok(deps.dep, 'dep present')
      done()
    })
  })
})

test('ls with filter arg', function (t) {
  var fixture = new Tacks(
    Dir({
      'npm-test-ls': Dir({
        'package.json': File({
          name: 'npm-test-ls',
          version: '1.0.0',
          dependencies: {
            'dep': 'file:../dep'
          }
        })
      }),
      'dep': Dir({
        'package.json': File({
          name: 'dep',
          version: '1.0.0'
        })
      }),
      'otherdep': Dir({
        'package.json': File({
          name: 'otherdep',
          version: '1.0.0'
        })
      })
    })
  )
  withFixture(t, fixture, function (done) {
    common.npm([
      'ls', 'dep',
      '--json'
    ], {
      cwd: path.join(fixturepath, 'npm-test-ls')
    }, function (err, code, stdout, stderr) {
      t.ifErr(err, 'ls succeeded')
      t.equal(0, code, 'exit 0 on ls')
      var pkg = JSON.parse(stdout)
      var deps = pkg.dependencies
      t.ok(deps.dep, 'dep present')
      t.notOk(deps.otherdep, 'other dep not present')
      done()
    })
  })
})

test('ls with missing filtered arg', function (t) {
  var fixture = new Tacks(
    Dir({
      'npm-test-ls': Dir({
        'package.json': File({
          name: 'npm-test-ls',
          version: '1.0.0',
          dependencies: {
            'dep': 'file:../dep'
          }
        })
      }),
      'dep': Dir({
        'package.json': File({
          name: 'dep',
          version: '1.0.0'
        })
      })
    })
  )
  withFixture(t, fixture, function (done) {
    common.npm([
      'ls', 'notadep',
      '--json'
    ], {
      cwd: path.join(fixturepath, 'npm-test-ls')
    }, function (err, code, stdout, stderr) {
      t.ifErr(err, 'ls succeeded')
      t.equal(1, code, 'exit 1 on ls')
      var pkg = JSON.parse(stdout)
      var deps = pkg.dependencies
      t.notOk(deps, 'deps missing')
      t.done()
    })
  })
})

test('ls with prerelease pkg', function (t) {
  var fixture = new Tacks(
    Dir({
      'npm-test-ls': Dir({
        'package.json': File({
          name: 'npm-test-ls',
          version: '1.0.0',
          dependencies: {
            'dep': 'file:../dep'
          }
        })
      }),
      'dep': Dir({
        'package.json': File({
          name: 'dep',
          version: '1.0.0-pre'
        })
      })
    })
  )
  withFixture(t, fixture, function (done) {
    common.npm([
      'ls', 'dep',
      '--json'
    ], {
      cwd: path.join(fixturepath, 'npm-test-ls')
    }, function (err, code, stdout, stderr) {
      t.ifErr(err, 'ls succeeded')
      t.equal(0, code, 'exit 0 on ls')
      var pkg = JSON.parse(stdout)
      var deps = pkg.dependencies
      t.ok(deps.dep, 'dep present')
      t.done()
    })
  })
})

test('cleanup', function (t) {
  rimraf.sync(basepath)
  t.done()
})

function withFixture (t, fixture, tester) {
  fixture.create(fixturepath)
  common.npm(['install'], {
    cwd: path.join(fixturepath, 'npm-test-ls')
  }, checkAndTest)
  function checkAndTest (err, code) {
    if (err) throw err
    t.is(code, 0, 'install went ok')
    tester(removeAndDone)
  }
  function removeAndDone (err) {
    if (err) throw err
    fixture.remove(fixturepath)
    rimraf.sync(basepath)
    t.done()
  }
}
