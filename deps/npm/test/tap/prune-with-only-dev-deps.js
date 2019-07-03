var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap')
var server

var pkg = common.pkg
var cache = path.resolve(pkg, 'cache')

var json = {
  name: 'prune-with-only-dev-deps',
  description: 'fixture',
  version: '0.0.1',
  main: 'index.js',
  devDependencies: {
    'test-package-with-one-dep': '0.0.0',
    'test-package': '0.0.0'
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
    t.ifErr(err, 'install finished successfully')
    t.notOk(code, 'exit ok')
    t.notOk(stderr, 'Should not get data on stderr: ' + stderr)
    t.end()
  })
})

function readdir (dir) {
  try {
    return fs.readdirSync(dir)
  } catch (ex) {
    if (ex.code === 'ENOENT') return []
    throw ex
  }
}

test('verify installs', function (t) {
  var dirs = readdir(pkg + '/node_modules').sort()
  t.same(dirs, [ 'test-package', 'test-package-with-one-dep' ].sort())
  t.end()
})

test('npm prune', function (t) {
  common.npm([
    'prune',
    '--loglevel', 'silent',
    '--production', 'false'
  ], EXEC_OPTS, function (err, code, stdout, stderr) {
    t.ifErr(err, 'prune finished successfully')
    t.notOk(code, 'exit ok')
    t.notOk(stderr, 'Should not get data on stderr: ' + stderr)
    t.end()
  })
})

test('verify installs', function (t) {
  var dirs = readdir(pkg + '/node_modules').sort()
  t.same(dirs, [ 'test-package', 'test-package-with-one-dep' ])
  t.end()
})

test('npm prune', function (t) {
  common.npm([
    'prune',
    '--loglevel', 'silent',
    '--production',
    '--json'
  ], EXEC_OPTS, function (err, code, stdout, stderr) {
    t.ifErr(err, 'prune finished successfully')
    t.notOk(code, 'exit ok')
    t.like(JSON.parse(stdout), {removed: [{name: 'test-package'}, {name: 'test-package-with-one-dep'}]})
    t.end()
  })
})

test('verify installs', function (t) {
  var dirs = readdir(pkg + '/node_modules').sort()
  t.same(dirs, [])
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
