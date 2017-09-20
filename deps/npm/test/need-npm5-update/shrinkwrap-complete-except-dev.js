'use strict'
var fs = require('graceful-fs')
var path = require('path')
var test = require('tap').test
var mr = require('npm-registry-mock')
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir
var extend = Object.assign || require('util')._extend
var common = require('../common-tap.js')

var basedir = path.join(__dirname, path.basename(__filename, '.js'))
var testdir = path.join(basedir, 'testdir')
var cachedir = path.join(basedir, 'cache')
var globaldir = path.join(basedir, 'global')
var tmpdir = path.join(basedir, 'tmp')
var appdir = path.join(testdir, 'app')
var installedModuleA = path.join(appdir, 'node_modules', 'module-a')
var installedModuleB = path.join(installedModuleA, 'node_modules', 'module-b')
var installedModuleC = path.join(appdir, 'node_modules', 'module-c')
var installedModuleD = path.join(appdir, 'node_modules', 'module-d')

var conf = {
  cwd: appdir,
  env: extend(extend({}, process.env), {
    npm_config_cache: cachedir,
    npm_config_tmp: tmpdir,
    npm_config_prefix: globaldir,
    npm_config_registry: common.registry,
    npm_config_loglevel: 'warn'
  })
}

var server
var fixture = new Tacks(Dir({
  cache: Dir(),
  global: Dir(),
  tmp: Dir(),
  testdir: Dir({
    app: Dir({
      'npm-shrinkwrap.json': File({
        name: 'app',
        version: '1.0.0',
        dependencies: {
          'module-a': {
            version: '1.0.0',
            from: '../module-a',
            resolved: 'file:../module-a',
            dependencies: {
              'module-b': {
                version: '1.0.0',
                from: '../module-b',
                resolved: 'file:../module-b'
              }
            }
          }
        }
      }),
      'package.json': File({
        name: 'app',
        version: '1.0.0',
        dependencies: {
          'module-a': '../module-a'
        },
        devDependencies: {
          'module-d': '../module-d'
        }
      })
    }),
    'module-a': Dir({
      'package.json': File({
        name: 'module-a',
        version: '1.0.0',
        dependencies: {
          'module-b': 'file:' + path.join(testdir, 'module-b'),
          'module-c': 'file:' + path.join(testdir, 'module-c')
        }
      })
    }),
    'module-b': Dir({
      'package.json': File({
        name: 'module-b',
        version: '1.0.0'
      })
    }),
    'module-c': Dir({
      'package.json': File({
        name: 'module-c',
        version: '1.0.0'
      })
    }),
    'module-d': Dir({
      'package.json': File({
        name: 'module-d',
        version: '1.0.0'
      })
    })
  })
}))

function exists (t, filepath, msg) {
  try {
    fs.statSync(filepath)
    t.pass(msg)
    return true
  } catch (ex) {
    t.fail(msg, {found: ex, wanted: 'exists', compare: 'fs.stat(' + filepath + ')'})
    return false
  }
}

function notExists (t, filepath, msg) {
  try {
    fs.statSync(filepath)
    t.fail(msg, {found: true, wanted: 'does not exist', compare: 'fs.stat(' + filepath + ')'})
    return true
  } catch (ex) {
    t.pass(msg)
    return false
  }
}

function setup () {
  cleanup()
  fixture.create(basedir)
}

function cleanup () {
  fixture.remove(basedir)
}

test('setup', function (t) {
  setup()
  mr({port: common.port, throwOnUnmatched: true}, function (err, s) {
    if (err) throw err
    server = s
    t.done()
  })
})

test('example', function (t) {
  common.npm(['install'], conf, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'command ran ok')
    t.comment('1> ' + stdout.trim())
    t.comment('2> ' + stderr.trim())
    exists(t, installedModuleA, 'module-a: dep in shrinkwrap installed')
    exists(t, installedModuleB, 'module-b: nested dep from shrinkwrap nested')
    notExists(t, installedModuleC, 'module-c: dependency not installed because not in shrinkwrap')
    exists(t, installedModuleD, 'module-d: dev dependency installed despite not being in shrinkwrap')
    t.done()
  })
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.done()
})
