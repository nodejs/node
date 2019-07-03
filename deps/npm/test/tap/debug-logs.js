'use strict'
var path = require('path')
var test = require('tap').test
var Tacks = require('tacks')
var glob = require('glob')
var asyncMap = require('slide').asyncMap
var File = Tacks.File
var Dir = Tacks.Dir
var common = require('../common-tap.js')

var basedir = common.pkg
var testdir = path.join(basedir, 'testdir')
var cachedir = path.join(basedir, 'cache')
var globaldir = path.join(basedir, 'global')
var tmpdir = path.join(basedir, 'tmp')

var conf = {
  cwd: testdir,
  env: Object.assign({}, process.env, {
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

    // we run a bunch concurrently, this will actually create > than our limit
    // as the check is done when the command starts
    //
    // It has to be > the log count (10) but also significantly higher than
    // the number of cores on the machine, or else some might be able to get
    // to the log folder pruning logic in parallel, resulting in FEWER files
    // than we expect being present at the end.
    var procCount = Math.max(require('os').cpus().length * 2, 12)
    var todo = new Array(procCount).join(',').split(',').map((v, k) => k)
    asyncMap(todo, function (num, next) {
      // another way would be to just simulate this?
      // var f = path.join(cachedir, '_logs', num + '-debug.log')
      // require('fs').writeFile(f, 'log ' + num, next)
      common.npm(['run', '--logs-max=10', 'false'], conf, function (err, code) {
        if (err) throw err
        t.is(code, 1, 'run #' + num + ' errored as expected')
        next()
      })
    }, function () {
      var files = glob.sync(path.join(cachedir, '_logs', '*'))
      t.ok(files.length > 10, 'there should be more than 10 log files')

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
