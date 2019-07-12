var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = common.pkg

var EXEC_OPTS = { cwd: pkg }

function hasControlCodes (str) {
  return str.length !== ansiTrim(str).length
}

function ansiTrim (str) {
  var r = new RegExp('\x1b(?:\\[(?:\\d+[ABCDEFGJKSTm]|\\d+;\\d+[Hfm]|' +
        '\\d+;\\d+;\\d+m|6n|s|u|\\?25[lh])|\\w)', 'g')
  return str.replace(r, '')
}

var json = {
  name: 'outdated-color',
  description: 'fixture',
  version: '0.0.1',
  dependencies: {
    underscore: '1.3.1'
  }
}

test('setup', function (t) {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )

  process.chdir(pkg)
  t.end()
})

// note hard to automate tests for color = true
// as npm kills the color config when it detects
// it's not running in a tty
test('does not use ansi styling', function (t) {
  t.plan(4)
  mr({ port: common.port }, function (er, s) { // create mock registry.
    common.npm(
      [
        '--registry', common.registry,
        'outdated', 'underscore'
      ],
      EXEC_OPTS,
      function (err, code, stdout) {
        t.ifError(err)
        t.is(code, 1, 'npm outdated exited with code 1')
        t.ok(stdout, stdout.length)
        t.ok(!hasControlCodes(stdout))
        s.close()
      })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  rimraf.sync(pkg)
}
