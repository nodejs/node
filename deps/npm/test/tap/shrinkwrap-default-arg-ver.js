'use strict'
var path = require('path')
var fs = require('graceful-fs')
var test = require('tap').test
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir
var common = require('../common-tap.js')
var mr = require('npm-registry-mock')

var testdir = path.join(__dirname, path.basename(__filename, '.js'))
var config = [
  '--loglevel=error',
  '--registry=' + common.registry,
  '--cache=' + path.join(testdir, 'cache')
]

var fixture = new Tacks(
  Dir({
    'cache': Dir(),
    'npm-shrinkwrap.json': File({
      name: 'shrinkwrap-default-arg-ver',
      version: '1.0.0',
      dependencies: {
        underscore: {
          version: '1.3.1',
          from: 'mod1@>=1.3.1 <2.0.0',
          resolved: common.registry + '/underscore/-/underscore-1.3.1.tgz'
        }
      }
    }),
    'package.json': File({
      name: 'shrinkwrap-default-arg-ver',
      version: '1.0.0',
      dependencies: {
        underscore: '^1.3.1'
      }
    })
  })
)
var installed = path.join(testdir, 'node_modules', 'underscore', 'package.json')

function setup () {
  fixture.create(testdir)
}

function cleanup () {
  fixture.remove(testdir)
}

var server
test('setup', function (t) {
  cleanup()
  setup()
  mr({port: common.port}, function (er, s) {
    if (er) throw er
    server = s
    t.end()
  })
})

function exists (file) {
  try {
    fs.statSync(file)
    return true
  } catch (ex) {
    return false
  }
}
test('shrinkwrap-default-arg-version', function (t) {
  // When this feature was malfunctioning npm would select the version of
  // `mod1` from the `package.json` instead of the `npm-shrinkwrap.json`,
  // which in this case would mean trying the registry instead of installing
  // from a local folder.
  common.npm(config.concat(['install', 'underscore']), {cwd: testdir}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    t.is(code, 0, 'installed ok')
    t.ok(exists(path.join(testdir, 'node_modules', 'underscore')), 'underscore installed')
    var pjson = JSON.parse(fs.readFileSync(installed))
    t.is(pjson.version, '1.3.1', 'got shrinkwrap version')
    t.end()
  })
})

test('can-override', function (t) {
  common.npm(config.concat(['install', 'underscore@latest']), {cwd: testdir}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    t.is(code, 0, 'installed ok')
    t.ok(exists(path.join(testdir, 'node_modules', 'underscore')), 'underscore installed')
    var pjson = JSON.parse(fs.readFileSync(installed))
    t.is(pjson.version, '1.5.1', 'got latest version')
    t.end()
  })
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.end()
})
