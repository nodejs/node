var fs = require('fs')
var resolve = require('path').resolve

var osenv = require('osenv')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = resolve(__dirname, 'graceful-restart')

var outGraceless = [
  'prerestart',
  'prestop',
  'stop',
  'poststop',
  'prestart',
  'start',
  'poststart',
  'postrestart',
  ''
].join('\n')

var outGraceful = [
  'prerestart',
  'restart',
  'postrestart',
  ''
].join('\n')

var pjGraceless = JSON.stringify({
  name: 'graceless',
  version: '1.2.3',
  scripts: {
    'prestop': 'echo prestop',
    'stop': 'echo stop',
    'poststop': 'echo poststop',
    'prerestart': 'echo prerestart',
    'postrestart': 'echo postrestart',
    'prestart': 'echo prestart',
    'start': 'echo start',
    'poststart': 'echo poststart'
  }
}, null, 2) + '\n'

var pjGraceful = JSON.stringify({
  name: 'graceful',
  version: '1.2.3',
  scripts: {
    'prestop': 'echo prestop',
    'stop': 'echo stop',
    'poststop': 'echo poststop',
    'prerestart': 'echo prerestart',
    'restart': 'echo restart',
    'postrestart': 'echo postrestart',
    'prestart': 'echo prestart',
    'start': 'echo start',
    'poststart': 'echo poststart'
  }
}, null, 2) + '\n'

test('setup', function (t) {
  bootstrap()
  t.end()
})

test('graceless restart', function (t) {
  fs.writeFileSync(resolve(pkg, 'package.json'), pjGraceless)
  createChild(['run-script', 'restart'], function (err, code, out) {
    t.ifError(err, 'restart finished successfully')
    t.equal(code, 0, 'npm run-script exited with code')
    t.equal(out, outGraceless, 'expected all scripts to run')
    t.end()
  })
})

test('graceful restart', function (t) {
  fs.writeFileSync(resolve(pkg, 'package.json'), pjGraceful)
  createChild(['run-script', 'restart'], function (err, code, out) {
    t.ifError(err, 'restart finished successfully')
    t.equal(code, 0, 'npm run-script exited with code')
    t.equal(out, outGraceful, 'expected only *restart scripts to run')
    t.end()
  })
})

test('clean', function (t) {
  cleanup()
  t.end()
})

function bootstrap () {
  mkdirp.sync(pkg)
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}

function createChild (args, cb) {
  var env = {
    HOME: process.env.HOME,
    Path: process.env.PATH,
    PATH: process.env.PATH,
    'npm_config_loglevel': 'silent'
  }

  if (process.platform === 'win32') {
    env.npm_config_cache = '%APPDATA%\\npm-cache'
  }

  return common.npm(args, {
    cwd: pkg,
    stdio: ['ignore', 'pipe', 'ignore'],
    env: env
  }, cb)
}
