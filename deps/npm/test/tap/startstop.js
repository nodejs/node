var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap')

var pkg = path.resolve(__dirname, 'startstop')

var EXEC_OPTS = { cwd: pkg }

var json = {
  name: 'startstop',
  version: '1.2.3',
  scripts: {
    start: 'node -e "console.log(\'start\')"',
    stop: 'node -e "console.log(\'stop\')"'
  }
}

function testOutput (t, command, er, code, stdout, stderr) {
  t.notOk(code, 'npm ' + command + ' exited with code 0')

  if (stderr) throw new Error('npm ' + command + ' stderr: ' + stderr.toString())

  stdout = stdout.trim().split(/\n|\r/)
  stdout = stdout[stdout.length - 1]
  t.equal(stdout, command)
  t.end()
}

test('setup', function (t) {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  t.end()
})

test('npm start', function (t) {
  common.npm(['start'], EXEC_OPTS, testOutput.bind(null, t, 'start'))
})

test('npm stop', function (t) {
  common.npm(['stop'], EXEC_OPTS, testOutput.bind(null, t, 'stop'))
})

test('npm restart', function (t) {
  common.npm(['restart'], EXEC_OPTS, function (er, c, stdout) {
    if (er) throw er

    var output = stdout.split('\n').filter(function (val) {
      return val.match(/^s/)
    })

    t.same(output.sort(), ['start', 'stop'].sort())
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
