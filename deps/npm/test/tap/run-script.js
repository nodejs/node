var common = require('../common-tap')
  , test = require('tap').test
  , path = require('path')
  , spawn = require('child_process').spawn
  , rimraf = require('rimraf')
  , mkdirp = require('mkdirp')
  , pkg = __dirname + '/run-script'
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

test('npm run-script', function (t) {
  common.npm(['run-script', 'start'], opts, testOutput.bind(null, t, "start"))
})

test('npm run-script with args', function (t) {
  common.npm(["run-script", "start", "--", "stop"], opts, testOutput.bind(null, t, "stop"))
})

test('npm run-script with args that contain spaces', function(t) {
  common.npm(["run-script", "start", "--", "hello world"], opts, testOutput.bind(null, t, "hello world"))
})

test('npm run-script with args that contain single quotes', function(t) {
  common.npm(["run-script", "start", "--", "they're awesome"], opts, testOutput.bind(null, t, "they're awesome"))
})

test('npm run-script with args that contain double quotes', function(t) {
  common.npm(["run-script", "start", "--", "what's \"up\"?"], opts, testOutput.bind(null, t, "what's \"up\"?"))
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})
