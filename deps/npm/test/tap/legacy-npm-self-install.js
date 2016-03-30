'use strict'
var test = require('tap').test
var fs = require('graceful-fs')
var common = require('../common-tap.js')
var path = require('path')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var osenv = require('osenv')
var npmpath = path.resolve(__dirname, '../..')
var basepath = path.resolve(osenv.tmpdir(), path.basename(__filename, '.js'))
var globalpath = path.resolve(basepath, 'global')
var extend = Object.assign || require('util')._extend
var isWin32 = process.platform === 'win32'

test('setup', function (t) {
  setup()
  t.done()
})

var tarball

test('build-tarball', function (t) {
  common.npm(['pack'], {cwd: npmpath, stdio: ['ignore', 'pipe', process.stderr]}, function (err, code, stdout) {
    if (err) throw err
    t.is(code, 0, 'pack went ok')
    tarball = path.resolve(npmpath, stdout.trim().replace(/^(?:.|\n)*(?:^|\n)(.*?[.]tgz)$/, '$1'))
    t.match(tarball, /[.]tgz$/, 'got a tarball')
    t.done()
  })
})

function exists () {
  try {
    fs.statSync(path.resolve.apply(null, arguments))
    return true
  } catch (ex) {
    return false
  }
}

test('npm-self-install', function (t) {
  if (!tarball) return t.done()

  var env = extend({}, process.env)
  var pathsep = isWin32 ? ';' : ':'
  env.npm_config_prefix = globalpath
  env.npm_config_global = 'true'
  env.npm_config_npat = 'false'
  env.NODE_PATH = null
  env.npm_config_user_agent = null
  env.npm_config_color = 'always'
  env.npm_config_progress = 'always'
  var PATH = env.PATH.split(pathsep)
  var binpath = isWin32 ? globalpath : path.join(globalpath, 'bin')
  var cmdname = isWin32 ? 'npm.cmd' : 'npm'
  PATH.unshift(binpath)
  env.PATH = PATH.join(pathsep)

  var opts = {cwd: basepath, env: env, stdio: ['ignore', 'ignore', process.stderr]}

  common.npm(['install', '--ignore-scripts', tarball], opts, installCheckAndTest)
  function installCheckAndTest (err, code) {
    if (err) throw err
    t.is(code, 0, 'install went ok')
    t.is(exists(binpath, cmdname), true, 'binary was installed')
    t.is(exists(globalpath, 'lib', 'node_modules', 'npm'), true, 'module path exists')
    common.npm(['ls', '--json', '--depth=0'], {cwd: basepath, env: env}, lsCheckAndRemove)
  }
  function lsCheckAndRemove (err, code, stdout, stderr) {
    t.ifError(err, 'npm test on array bin')
    t.equal(code, 0, 'exited OK')
    t.equal(stderr.trim(), '', 'no error output')
    var installed = JSON.parse(stdout.trim())
    t.is(Object.keys(installed.dependencies).length, 1, 'one thing installed')
    t.is(path.resolve(globalpath, installed.dependencies.npm.from), tarball, 'and it was our npm tarball')
    common.npm(['rm', 'npm'], {cwd: basepath, env: env, stdio: 'inherit'}, removeCheck)
  }
  function removeCheck (err, code) {
    if (err) throw err
    t.is(code, 0, 'remove went ok')
    common.npm(['ls', '--json', '--depth=0'], {cwd: basepath, env: env}, andDone)
  }
  function andDone (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'remove went ok')
    t.equal(stderr.trim(), '', 'no error output')
    var installed = JSON.parse(stdout.trim())
    t.ok(!installed.dependencies || installed.dependencies.length === 0, 'nothing left')
    t.is(exists(binpath, cmdname), false, 'binary was removed')
    t.is(exists(globalpath, 'lib', 'node_modules', 'npm'), false, 'module was entirely removed')
    t.done()
  }
})

test('cleanup', function (t) {
  cleanup()
  t.done()
})

function setup () {
  cleanup()
  mkdirp.sync(globalpath)
}

function cleanup () {
  rimraf.sync(basepath)
}
