'use strict'
var test = require('tap').test
var Bluebird = require('bluebird')
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir
var fs = require('fs')
var path = require('path')
var common = require('../common-tap.js')

var testdir = path.resolve(__dirname, path.basename(__filename, '.js'))
var modAtgz = path.resolve(testdir, 'modA') + '-1.0.0.tgz'
var modB1tgz = path.resolve(testdir, 'modB') + '-1.0.0.tgz'
var modB2tgz = path.resolve(testdir, 'modB') + '-2.0.0.tgz'
var modCtgz = path.resolve(testdir, 'modC') + '-1.0.0.tgz'

var fixture = new Tacks(Dir({
  'package.json': File({
    dependencies: {
      modA: 'file://' + modAtgz,
      modC: 'file://' + modCtgz
    }
  }),
  'npm-shrinkwrap.json': File({
    dependencies: {
      modA: {
        version: '1.0.0',
        resolved: 'file://' + modAtgz
      },
      modB: {
        version: '1.0.0',
        resolved: 'file://' + modB1tgz
      }
    }
  }),
  'modA': Dir({
    'package.json': File({
      name: 'modA',
      version: '1.0.0',
      dependencies: {
        'modB': 'file://' + modB1tgz
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
        'modB': 'file://' + modB2tgz
      }
    })
  })
}))

var newShrinkwrap = new Tacks(Dir({
  'npm-shrinkwrap.json': File({
    dependencies: {
      modA: {
        version: '1.0.0',
        resolved: 'file://' + modAtgz
      },
      modB: {
        version: '1.0.0',
        resolved: 'file://' + modB1tgz
      },
      modC: {
        version: '1.0.0',
        resolved: 'file://' + modCtgz,
        dependencies: {
          modB: {
            version: '1.0.0',
            resolved: 'file://' + modB1tgz
          }
        }
      }
    }
  }),
  'node_modules': Dir({
    'modB@1': Dir({
      'package.json': File({
        _requested: {
          name: 'modB',
          raw: 'modB@file:' + modB1tgz,
          rawSpec: 'file:' + modB1tgz,
          scope: null,
          spec: modB1tgz,
          type: 'directory'
        },
        dependencies: { },
        devDependencies: { },
        name: 'modB',
        optionalDependencies: { },
        readme: 'ERROR: No README data found!',
        version: '1.0.0'
      })
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
  return Bluebird.try(() => {
    return Bluebird.join(
      common.npm(['pack', 'file:modB@1'], {cwd: testdir, stdio: [0, 2, 2]}),
      common.npm(['pack', 'file:modB@2'], {cwd: testdir, stdio: [0, 2, 2]}),
      function (b1, b2) {
        t.is(b1[0], 0, 'pack modB@1')
        t.is(b2[0], 0, 'pack modB@2')
      })
  }).then(() => {
    return Bluebird.join(
      common.npm(['pack', 'file:modA'], {cwd: testdir, stdio: [0, 2, 2]}),
      common.npm(['pack', 'file:modC'], {cwd: testdir, stdio: [0, 2, 2]}),
      function (a, c) {
        t.is(a[0], 0, 'pack modA')
        t.is(c[0], 0, 'pack modC')
      })
  }).then(() => {
    return common.npm(['install'], {cwd: testdir, stdio: [0, 2, 2]})
  }).spread((code) => {
    t.is(code, 0, 'top level install')
    t.end()
  })
})

test('incremental install', function (t) {
  newShrinkwrap.create(testdir)
  common.npm(['install'], {cwd: testdir, stdio: [0, 2, 2]}, function (err, code) {
    if (err) throw err
    t.is(code, 0, 'npm itself completed ok')
    fs.stat(path.join(testdir, 'node_modules', 'modC', 'node_modules', 'modB', 'B1'), function (missing) {
      t.ok(!missing, 'modC got the updated version of modB')
      t.end()
    })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})
