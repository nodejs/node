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

var newShrinkwrap = new Tacks(Dir({
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
      },
      modC: {
        version: '1.0.0',
        from: 'modC',
        resolved: 'file://' + modCdir,
        dependencies: {
          modB: {
            version: '1.0.0',
            from: 'modB@1',
            resolved: 'file://' + modB1dir
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
          raw: 'modB@file:' + modB1dir,
          rawSpec: 'file:' + modB1dir,
          scope: null,
          spec: modB1dir,
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
  common.npm(['install'], {cwd: testdir, stdio: [0, 2, 2]}, function (err, code) {
    if (err) throw err
    t.is(code, 0)
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
