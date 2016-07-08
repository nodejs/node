var test = require('tap').test
var fs = require('fs')
var path = require('path')
var common = require('../common-tap.js')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var pkg = path.resolve(__dirname, 'git-cache-no-hooks')
var osenv = require('osenv')
var tmp = path.join(pkg, 'tmp')
var cache = path.join(pkg, 'cache')

test('setup', function (t) {
  rimraf.sync(pkg)
  mkdirp.sync(pkg)
  mkdirp.sync(cache)
  mkdirp.sync(tmp)
  mkdirp.sync(path.resolve(pkg, 'node_modules'))
  t.end()
})

test('git-cache-no-hooks: install a git dependency', function (t) {
  // disable git integration tests on Travis.
  if (process.env.TRAVIS) return t.end()

  common.npm([
    'install', 'git://github.com/nigelzor/npm-4503-a.git',
    '--cache', cache,
    '--tmp', tmp
  ], {
    cwd: pkg
  }, function (err, code, stdout, stderr) {
    t.ifErr(err, 'npm completed')
    t.equal(stderr, '', 'no error output')
    t.equal(code, 0, 'npm install should succeed')

    // verify permissions on git hooks
    var repoDir = 'git-github-com-nigelzor-npm-4503-a-git-40c5cb24'
    var hooksPath = path.join(cache, '_git-remotes', repoDir, 'hooks')
    fs.readdir(hooksPath, function (err) {
      t.equal(err && err.code, 'ENOENT', 'hooks are not brought along with repo')
      t.end()
    })
  })
})

test('cleanup', function (t) {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
  t.end()
})
