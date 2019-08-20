var common = require('../common-tap.js')

var resolve = require('path').resolve
var fs = require('graceful-fs')
var test = require('tap').test
var rimraf = require('rimraf')
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir
var Symlink = Tacks.Symlink
var isWindows = require('../../lib/utils/is-windows.js')

var base = common.pkg
var fixture = new Tacks(Dir({
  'working-dir': Dir({
    'node_modules': Dir({}) // so it doesn't try to install into npm's own node_modules
  }),
  'test-module': Dir({
    'package.json': File({
      name: '@test/linked',
      version: '1.0.0',
      bin: {
        linked: './index.js'
      }
    }),
    'index.js': File("module.exports = function () { console.log('whoop whoop') }")
  }),
  'global-dir': Dir({}),
  'linked-global-dir': Symlink('global-dir')
}))

var workingDir = resolve(base, 'working-dir')
var toInstall = resolve(base, 'test-module')
var linkedGlobal = resolve(base, 'linked-global-dir')

var env = Object.assign({}, process.env)

// We set the global install location via env var here
// instead of passing it in via `--prefix` because
// `--prefix` ALSO changes the current package location.
// And we don't ue the PREFIX env var because
// npm_config_prefix takes precedence over it and is
// passed in when running from the npm test suite.
env.npm_config_prefix = linkedGlobal
var EXEC_OPTS = {
  cwd: workingDir,
  env: env,
  stdio: [0, 'pipe', 2]
}

test('setup', function (t) {
  cleanup()
  setup()

  t.end()
})

test('install and link', function (t) {
  var globalBin = resolve(linkedGlobal, isWindows ? '.' : 'bin', 'linked')
  var globalModule = resolve(linkedGlobal, isWindows ? '.' : 'lib', 'node_modules', '@test', 'linked')
  // link our test module into the global folder
  return common.npm(['--loglevel', 'error', 'link', toInstall], EXEC_OPTS).spread((code, out) => {
    t.comment(out)
    t.is(code, 0, 'link succeeded')
    var localBin = resolve(workingDir, 'node_modules', '.bin', 'linked')
    var localModule = resolve(workingDir, 'node_modules', '@test', 'linked')
    try {
      t.ok(fs.statSync(globalBin), 'global bin exists')
      t.is(fs.lstatSync(globalModule).isSymbolicLink(), true, 'global module is link')
      t.ok(fs.statSync(localBin), 'local bin exists')
      t.is(fs.lstatSync(localModule).isSymbolicLink(), true, 'local module is link')
    } catch (ex) {
      t.ifError(ex, 'linking happened')
    }
    if (code !== 0) throw new Error('aborting')

    // and try removing it and make sure that succeeds
    return common.npm(['--global', '--loglevel', 'error', 'rm', '@test/linked'], EXEC_OPTS)
  }).spread((code, out) => {
    t.comment(out)
    t.is(code, 0, 'rm succeeded')
    t.throws(function () { fs.statSync(globalBin) }, 'global bin removed')
    t.throws(function () { fs.statSync(globalModule) }, 'global module removed')
  })
})

test('cleanup', function (t) {
  cleanup()

  t.end()
})

function cleanup () {
  fixture.remove(base)
  rimraf.sync(base)
}

function setup () {
  fixture.create(base)
}
