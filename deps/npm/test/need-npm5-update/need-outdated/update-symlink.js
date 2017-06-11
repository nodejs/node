var fs = require('graceful-fs')
var path = require('path')
var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var testdir = path.join(__dirname, path.basename(__filename, '.js'))
var pkg = path.resolve(testdir, 'update-symlink')
var cachedir = path.join(testdir, 'cache')
var globaldir = path.join(testdir, 'global')
var OPTS = {
  env: {
    'npm_config_cache': cachedir,
    'npm_config_prefix': globaldir,
    'npm_config_registry': common.registry
  },
  cwd: pkg
}

var jsonLocal = {
  name: 'my-local-package',
  description: 'fixture',
  version: '1.0.0',
  dependencies: {
    'async': '*',
    'underscore': '*'
  }
}

var server
test('setup', function (t) {
  cleanup()
  mkdirp.sync(cachedir)
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(jsonLocal, null, 2)
  )

  mr({ port: common.port }, thenInstallUnderscore)

  function thenInstallUnderscore (er, s) {
    server = s
    common.npm(['install', '-g', 'underscore@1.3.1'], OPTS, thenLink)
  }

  function thenLink (err, c, out) {
    t.ifError(err, 'global install did not error')
    common.npm(['link', 'underscore'], OPTS, thenInstallAsync)
  }

  function thenInstallAsync (err, c, out) {
    t.ifError(err, 'link did not error')
    common.npm(['install', 'async@0.2.9'], OPTS, thenVerify)
  }

  function thenVerify (err, c, out) {
    t.ifError(err, 'local install did not error')
    common.npm(['ls'], OPTS, function (err, c, out, stderr) {
      t.ifError(err)
      t.equal(c, 0)
      t.equal(stderr, '', 'got expected stderr')
      t.has(out, /async@0.2.9/, 'installed ok')
      t.has(out, /underscore@1.3.1/, 'creates local link ok')
      t.end()
    })
  }
})

test('when update is called linked packages should be excluded', function (t) {
  common.npm(['update', '--parseable'], OPTS, function (err, c, out, stderr) {
    if (err) throw err
    t.equal(c, 0)
    t.has(out, /^update\tasync\t0.2.10\t/m, 'updated ok')
    t.doesNotHave(stderr, /ERR!/, 'no errors in stderr')
    t.end()
  })
})

test('when install is called and the package already exists as a link, outputs a warning if the requested version is not the same as the linked one', function (t) {
  common.npm(['install', 'underscore', '--parseable'], OPTS, function (err, c, out, stderr) {
    if (err) throw err
    t.equal(c, 0)

    t.comment(out.trim())
    t.comment(stderr.trim())
    t.has(out, /^update-linked\tunderscore\t/m)
    t.has(stderr, /underscore/, 'warning output relating to linked package')
    t.doesNotHave(stderr, /ERR!/, 'no errors in stderr')
    t.end()
  })
})

test('when install is called and the package already exists as a link, does not warn if the requested version is same as the linked one', function (t) {
  common.npm(['install', 'underscore@1.3.1', '--parseable'], OPTS, function (err, c, out, stderr) {
    if (err) throw err
    t.equal(c, 0)
    t.comment(out.trim())
    t.comment(stderr.trim())
    t.has(out, /^update-linked\tunderscore\t/m)
    t.doesNotHave(stderr, /underscore/, 'no warning or error relating to linked package')
    t.doesNotHave(stderr, /ERR!/, 'no errors in stderr')
    t.end()
  })
})

test('cleanup', function (t) {
  server.close()
  common.npm(['rm', 'underscore', 'async'], OPTS, function (err, code) {
    if (err) throw err
    t.equal(code, 0, 'cleanup in local ok')
    common.npm(['rm', '-g', 'underscore'], OPTS, function (err, code) {
      if (err) throw err
      t.equal(code, 0, 'cleanup in global ok')

      cleanup()
      t.end()
    })
  })
})

function cleanup () {
  rimraf.sync(testdir)
}
