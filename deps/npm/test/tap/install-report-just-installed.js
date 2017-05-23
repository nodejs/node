'use strict'
var path = require('path')
var test = require('tap').test
var Tacks = require('tacks')
var Dir = Tacks.Dir
var File = Tacks.File
var common = require('../common-tap.js')

var testdir = path.resolve(__dirname, path.basename(__filename, '.js'))
var fixture = new Tacks(Dir({
  node_modules: Dir({
    a: Dir({
      'package.json': File({
        name: 'a',
        version: '1.0.0',
        dependencies: {
          b: '1.0.0'
        }
      }),
      node_modules: Dir({
        b: Dir({
          'package.json': File({
            name: 'b',
            version: '1.0.0'
          })
        })
      })
    })
  }),
  'b-src': Dir({
    'package.json': File({
      name: 'b',
      version: '1.0.0'
    })
  })
}))

function setup () {
  cleanup()
  fixture.create(testdir)
}

function cleanup () {
  fixture.remove(testdir)
}

test('setup', function (t) {
  setup()
  t.end()
})

test('install-report', function (t) {
  common.npm(['install', '--no-save', '--json', './b-src'], {cwd: testdir}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'installed successfully')
    t.is(stderr, '', 'no warnings')
    try {
      var report = JSON.parse(stdout)
      t.pass('stdout was json')
    } catch (ex) {
      t.fail('stdout was json')
      console.error(ex)
      t.skip(2)
      return t.end()
    }
    t.is(report.added.length, 1, 'one dependency reported as installed')
    t.match(report.added, [{name: 'b'}], 'that dependency was `b`')
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})
