var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap')
var server

var pkg = path.resolve(__dirname, 'prune')
var cache = path.resolve(pkg, 'cache')

var json = {
  name: 'prune-with-dev-dep-duplicate',
  description: 'fixture',
  version: '0.0.1',
  main: 'index.js',
  dependencies: {
    'test-package': '0.0.0'
  },
  devDependencies: {
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

function readdir (dir) {
  try {
    return fs.readdirSync(dir)
  } catch (ex) {
    if (ex.code === 'ENOENT') return []
    throw ex
  }
}

test('npm install also=dev', function (t) {
  common.npm([
    'install',
    '--cache', cache,
    '--registry', common.registry,
    '--loglevel', 'silent',
    '--also=dev'
  ], EXEC_OPTS, function (err, code, stdout, stderr) {
    if (err) throw err
    t.notOk(code, 'exit ok')
    t.notOk(stderr, 'Should not get data on stderr: ' + stderr)
    var dirs = readdir(pkg + '/node_modules').sort()
    t.same(dirs, [ 'test-package' ].sort())
    t.end()
  })
})

test('npm prune also=dev', function (t) {
  common.npm([
    'prune',
    '--loglevel', 'silent',
    '--also=dev'
  ], EXEC_OPTS, function (err, code, stdout, stderr) {
    if (err) throw err
    t.notOk(code, 'exit ok')
    t.notOk(stderr, 'Should not get data on stderr: ' + stderr)
    var dirs = readdir(pkg + '/node_modules').sort()
    t.same(dirs, [ 'test-package' ])
    t.end()
  })
})

test('npm prune only=prod', function (t) {
  common.npm([
    'prune',
    '--loglevel', 'silent',
    '--only=prod',
    '--json'
  ], EXEC_OPTS, function (err, code, stdout, stderr) {
    if (err) throw err
    t.notOk(code, 'exit ok')
    t.isDeeply(JSON.parse(stdout).removed, [], 'removed nothing')
    var dirs = readdir(pkg + '/node_modules').sort()
    t.same(dirs, [ 'test-package' ])
    t.end()
  })
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
