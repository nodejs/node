var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap')
var server

var pkg = path.resolve(__dirname, path.basename(__filename, '.js'))
var cache = path.resolve(pkg, 'cache')

var json = {
  name: 'prune',
  description: 'fixture',
  version: '0.0.1',
  main: 'index.js',
  dependencies: {
    underscore: '1.3.1'
  },
  devDependencies: {
    mkdirp: '*'
  }
}

var EXEC_OPTS = {
  cwd: pkg,
  npm_config_depth: 'Infinity'
}

test('setup', function (t) {
  cleanup()
  mkdirp.sync(cache)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  mr({ port: common.port }, function (er, s) {
    server = s
    t.end()
  })
})

test('npm install', function (t) {
  common.npm([
    'install',
    '--cache', cache,
    '--registry', common.registry,
    '--loglevel', 'silent',
    '--production', 'false'
  ], EXEC_OPTS, function (err, code, stdout, stderr) {
    if (err) throw err
    t.notOk(code, 'exit ok')
    t.notOk(stderr, 'Should not get data on stderr: ' + stderr)
    t.end()
  })
})

test('npm install test-package', function (t) {
  common.npm([
    'install', 'test-package',
    '--cache', cache,
    '--registry', common.registry,
    '--no-save',
    '--loglevel', 'silent',
    '--production', 'false'
  ], EXEC_OPTS, function (err, code, stdout, stderr) {
    if (err) throw err
    t.notOk(code, 'exit ok')
    t.notOk(stderr, 'Should not get data on stderr: ' + stderr)
    t.end()
  })
})

test('setup: verify installs', function (t) {
  var dirs = fs.readdirSync(pkg + '/node_modules').sort()
  t.same(dirs, [ 'test-package', 'mkdirp', 'underscore' ].sort())
  t.end()
})

test('dev: npm prune', function (t) {
  common.npm([
    'prune',
    '--loglevel', 'silent',
    '--production', 'false'
  ], EXEC_OPTS, function (err, code, stdout, stderr) {
    if (err) throw err
    t.notOk(code, 'exit ok')
    t.notOk(stderr, 'Should not get data on stderr: ' + stderr)
    t.end()
  })
})

test('dev: verify installs', function (t) {
  var dirs = fs.readdirSync(pkg + '/node_modules').sort()
  t.same(dirs, [ 'mkdirp', 'underscore' ])
  t.end()
})

test('production: npm prune', function (t) {
  common.npm([
    'prune',
    '--loglevel', 'silent',
    '--parseable',
    '--production'
  ], EXEC_OPTS, function (err, code, stdout) {
    if (err) throw err
    t.notOk(code, 'exit ok')
    t.equal(stdout.trim(), 'remove\tmkdirp\t0.3.5\tnode_modules/mkdirp')
    t.end()
  })
})

test('pruduction: verify installs', function (t) {
  var dirs = fs.readdirSync(pkg + '/node_modules').sort()
  t.same(dirs, [ 'underscore' ])
  t.end()
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.pass('cleaned up')
  t.end()
})

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
