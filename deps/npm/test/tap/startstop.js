var common = require('../common-tap')
  , test = require('tap').test
  , path = require('path')
  , spawn = require('child_process').spawn
  , rimraf = require('rimraf')
  , mkdirp = require('mkdirp')
  , pkg = __dirname + '/startstop'
  , cache = pkg + '/cache'
  , tmp = pkg + '/tmp'
  , node = process.execPath
  , npm = path.resolve(__dirname, '../../cli.js')
  , opts = { cwd: pkg }

function testOutput (t, command, er, code, stdout, stderr) {
  if (er)
    throw er

  if (stderr)
    throw new Error('npm ' + command + ' stderr: ' + stderr.toString())

  stdout = stdout.trim().split('\n')
  stdout = stdout[stdout.length - 1]
  t.equal(stdout, command)
  t.end()
}

function cleanup () {
  rimraf.sync(pkg + '/cache')
  rimraf.sync(pkg + '/tmp')
}

test('setup', function (t) {
  cleanup()
  mkdirp.sync(pkg + '/cache')
  mkdirp.sync(pkg + '/tmp')
  t.end()
})

test('npm start', function (t) {
  common.npm(['start'], opts, testOutput.bind(null, t, "start"))
})

test('npm stop', function (t) {
  common.npm(['stop'], opts, testOutput.bind(null, t, "stop"))
})

test('npm restart', function (t) {
  common.npm(['restart'], opts, function (er, c, stdout, stderr) {
    if (er)
      throw er

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
