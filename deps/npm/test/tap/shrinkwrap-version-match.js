'use strict'
var test = require('tap').test
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir
var fs = require('fs')
var path = require('path')
var common = require('../common-tap.js')

var testdir = path.resolve(__dirname, path.basename(__filename, '.js'))
var modAdir = path.resolve(testdir, 'modA')
var modB1dir = path.resolve(testdir, 'modB@1')
var modB2dir = path.resolve(testdir, 'modB@2')
var modCdir = path.resolve(testdir, 'modC')

var fixture = new Tacks(Dir({
  'package.json': File({
    dependencies: {
      modA: 'file://' + modAdir,
      modC: 'file://' + modCdir
    }
  }),
  'npm-shrinkwrap.json': File({
    dependencies: {
      modA: {
        version: '1.0.0',
        from: 'modA',
        resolved: 'file://' + modAdir
      },
      modB: {
        version: '1.0.0',
        from: 'modB@1',
        resolved: 'file://' + modB1dir
      }
    }
  }),
  'modA': Dir({
    'package.json': File({
      name: 'modA',
      version: '1.0.0',
      dependencies: {
        'modB': 'file://' + modB1dir
      }
    })
  }),
  'modB@1': Dir({
    'package.json': File({
      name: 'modB',
      version: '1.0.0'
    }),
    'B1': File('')
  }),
  'modB@2': Dir({
    'package.json': File({
      name: 'modB',
      version: '2.0.0'
    }),
    'B2': File('')
  }),
  'modC': Dir({
    'package.json': File({
      name: 'modC',
      version: '1.0.0',
      dependencies: {
        'modB': 'file://' + modB2dir
      }
    })
  })
}))

function setup () {
  fixture.create(testdir)
}

function cleanup () {
  fixture.remove(testdir)
}

test('setup', function (t) {
  cleanup()
  setup()
  t.end()
})

// Shrinkwraps need to let you override dependency versions specified in
// package.json files.  Indeed, this was already supported, but it was a bit
// to keen on this.  Previously, if you had a dep in your shrinkwrap then
// anything that required that dependency would count as a match, regardless
// of version.

// This test ensures that the broad matching is not done when the matched
// module is not a direct child of the module doing the requiring.

test('bundled', function (t) {
  common.npm(['install'], {cwd: testdir}, function (err, code, out, stderr) {
    t.is(err, null, 'No fatal errors running npm')
    t.is(code, 0, 'npm itself completed ok')
    // Specifically, if B2 exists (or the modB directory under modC at all)
    // that means modC was given its own copy of modB.  Without the patch
    // that went with this test, it wouldn't have been installed because npm
    // would have consider modB@1 to have fulfilled modC's requirement.
    fs.stat(path.join(testdir, 'node_modules', 'modC', 'node_modules', 'modB', 'B2'), function (missing) {
      t.ok(!missing, 'modC got the right version of modB')
      t.end()
    })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})
