var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var test = require('tap').test

var common = require('../common-tap.js')

// Install from a tarball that thinks it is underscore@1.5.1
// (but is actually a fork)
var forkPath = path.resolve(
  __dirname, '..', 'fixtures', 'forked-underscore-1.5.1.tgz'
)
var pkg = common.pkg
var cache = common.cache
var server

test('setup', function (t) {
  mkdirp.sync(path.join(pkg, 'node_modules'))
  process.chdir(pkg)
  t.comment('test for https://github.com/npm/npm/issues/3265')
  mr({ port: common.port }, function (er, s) {
    server = s
    t.end()
  })
})

test('npm cache - install from fork', function (t) {
  common.npm(
    [
      '--loglevel', 'silent',
      '--json',
      '--registry', common.registry,
      'install', forkPath
    ],
    {
      cwd: pkg,
      env: { npm_config_cache: cache }
    },
    function (err, code, stdout, stderr) {
      t.ifErr(err, 'install finished without error')
      t.equal(stderr, '', 'Should not get data on stderr')
      t.equal(code, 0, 'install finished successfully')

      var deps = {}
      JSON.parse(stdout).added.forEach(function (dep) { deps[dep.name] = dep })
      t.equal(deps.underscore && deps.underscore.version, '1.5.1')
      var index = fs.readFileSync(
        path.join(pkg, 'node_modules', 'underscore', 'index.js'),
        'utf8'
      )
      t.equal(index, 'console.log("This is the fork");\n\n')
      t.end()
    }
  )
})

// Now install the real 1.5.1.
test('npm cache - install from origin', function (t) {
  common.npm(
    [
      '--loglevel', 'silent',
      '--json',
      '--registry', common.registry,
      'install', 'underscore'
    ],
    {
      cwd: pkg,
      env: { npm_config_cache: cache }
    },
    function (err, code, stdout, stderr) {
      t.ifErr(err, 'install finished without error')
      t.equal(code, 0, 'install finished successfully')
      t.notOk(stderr, 'Should not get data on stderr: ' + stderr)
      var deps = {}
      JSON.parse(stdout).updated.forEach(function (dep) { deps[dep.name] = dep })
      t.equal(deps.underscore && deps.underscore.version, '1.5.1')
      var index = fs.readFileSync(
        path.join(pkg, 'node_modules', 'underscore', 'index.js'),
        'utf8'
      )
      t.equal(index, 'module.exports = require(\'./underscore\');\n')
      t.end()
    }
  )
})

test('cleanup', function (t) {
  server.close()
  t.end()
})
