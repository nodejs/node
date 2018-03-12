'use strict'
var path = require('path')
var test = require('tap').test
var Tacks = require('tacks')
var glob = require('glob')
var asyncMap = require('slide').asyncMap
var File = Tacks.File
var Dir = Tacks.Dir
var extend = Object.assign || require('util')._extend
var common = require('../common-tap.js')

var basedir = path.join(__dirname, path.basename(__filename, '.js'))
var testdir = path.join(basedir, 'testdir')
var cachedir = path.join(basedir, 'cache')
var globaldir = path.join(basedir, 'global')
var tmpdir = path.join(basedir, 'tmp')

var conf = {
  cwd: testdir,
  env: extend(extend({}, process.env), {
    npm_config_cache: cachedir,
    npm_config_tmp: tmpdir,
    npm_config_prefix: globaldir,
    npm_config_registry: common.registry,
    npm_config_loglevel: 'warn'
  })
}

var fixture = new Tacks(Dir({
  cache: Dir(),
  global: Dir(),
  tmp: Dir(),
  testdir: Dir({
    'package.json': File({
      name: 'debug-logs',
      version: '1.0.0',
      scripts: {
        true: 'node -e "process.exit(0)"',
        false: 'node -e "process.exit(1)"'
      }
    })
  })
}))

function setup () {
  cleanup()
  fixture.create(basedir)
}

function cleanup () {
  fixture.remove(basedir)
}

test('setup', function (t) {
  setup()
  t.done()
})

test('example', function (t) {
  common.npm(['run', 'false'], conf, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 1, 'command errored')
    var matches = stderr.match(/A complete log of this run can be found in:.*\nnpm ERR! {5,5}(.*)/)
    t.ok(matches, 'debug log mentioned in error message')
    if (matches) {
      var logfile = matches[1]
      t.matches(path.relative(cachedir, logfile), /^_logs/, 'debug log is inside the cache in _logs')
    }

    // we run a bunch concurrently, this will actually create > than our limit as the check is done
    // when the command starts
    var todo = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]
    asyncMap(todo, function (num, next) {
      common.npm(['run', '--logs-max=10', 'false'], conf, function (err, code) {
        if (err) throw err
        t.is(code, 1, 'run #' + num + ' errored as expected')
        next()
      })
    }, function () {
      // now we do one more and that should clean up the list
      common.npm(['run', '--logs-max=10', 'false'], conf, function (err, code) {
        if (err) throw err
        t.is(code, 1, 'final run errored as expected')
        var files = glob.sync(path.join(cachedir, '_logs', '*'))
        t.is(files.length, 10, 'there should never be more than 10 log files')
        common.npm(['run', '--logs-max=5', 'true'], conf, function (err, code) {
          if (err) throw err
          t.is(code, 0, 'success run is ok')
          var files = glob.sync(path.join(cachedir, '_logs', '*'))
          t.is(files.length, 4, 'after success there should be logs-max - 1 log files')
          t.done()
        })
      })
    })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.done()
})

