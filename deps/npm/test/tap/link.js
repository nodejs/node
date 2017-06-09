var mkdirp = require('mkdirp')
var osenv = require('osenv')
var path = require('path')
var rimraf = require('rimraf')
var test = require('tap').test
var lstatSync = require('fs').lstatSync
var writeFileSync = require('fs').writeFileSync

var common = require('../common-tap.js')

var link = path.join(__dirname, 'link')
var linkScoped = path.join(__dirname, 'link-scoped')
var linkInstall = path.join(__dirname, 'link-install')
var linkInside = path.join(linkInstall, 'node_modules', 'inside')
var linkRoot = path.join(__dirname, 'link-root')

var config = 'prefix = ' + linkRoot
var configPath = path.join(link, '_npmrc')

var OPTS = {
  env: {
    'npm_config_userconfig': configPath
  }
}

var readJSON = {
  name: 'foo',
  version: '1.0.0',
  description: '',
  main: 'index.js',
  scripts: {
    test: 'echo \"Error: no test specified\" && exit 1'
  },
  author: '',
  license: 'ISC'
}

var readScopedJSON = {
  name: '@scope/foo',
  version: '1.0.0',
  description: '',
  main: 'index.js',
  scripts: {
    test: 'echo \"Error: no test specified\" && exit 1'
  },
  author: '',
  license: 'ISC'
}

var installJSON = {
  name: 'bar',
  version: '1.0.0',
  description: '',
  main: 'index.js',
  scripts: {
    test: 'echo \"Error: no test specified\" && exit 1'
  },
  author: '',
  license: 'ISC'
}

var insideInstallJSON = {
  name: 'inside',
  version: '1.0.0',
  description: '',
  main: 'index.js',
  scripts: {
    test: 'echo \"Error: no test specified\" && exit 1'
  },
  author: '',
  license: 'ISC'
}

test('setup', function (t) {
  setup()
  common.npm(['ls', '-g', '--depth=0'], OPTS, function (err, c, out) {
    t.ifError(err)
    t.equal(c, 0, 'set up ok')
    t.notOk(out.match(/UNMET DEPENDENCY foo@/), "foo isn't in global")
    t.end()
  })
})

test('create global link', function (t) {
  process.chdir(link)
  common.npm(['link'], OPTS, function (err, c, out) {
    t.ifError(err, 'link has no error')
    common.npm(['ls', '-g'], OPTS, function (err, c, out, stderr) {
      t.ifError(err)
      t.equal(c, 0)
      t.equal(stderr, '', 'got expected stderr')
      t.has(out, /foo@1.0.0/, 'creates global link ok')
      t.end()
    })
  })
})

test('create global inside link', function (t) {
  process.chdir(linkInside)
  common.npm(['link'], OPTS, function (err, c, out) {
    t.ifError(err, 'link has no error')
    common.npm(['ls', '-g'], OPTS, function (err, c, out, stderr) {
      t.ifError(err)
      t.equal(c, 0)
      t.equal(stderr, '', 'got expected stderr')
      t.has(out, /inside@1.0.0/, 'creates global inside link ok')
      t.end()
    })
  })
})

test('create scoped global link', function (t) {
  process.chdir(linkScoped)
  common.npm(['link'], OPTS, function (err, c, out) {
    t.ifError(err, 'link has no error')
    common.npm(['ls', '-g'], OPTS, function (err, c, out, stderr) {
      t.ifError(err)
      t.equal(c, 0)
      t.equal(stderr, '', 'got expected stderr')
      t.has(out, /@scope[/]foo@1.0.0/, 'creates global link ok')
      t.end()
    })
  })
})

test('link-install the package', function (t) {
  process.chdir(linkInstall)
  common.npm(['link', 'foo'], OPTS, function (err) {
    t.ifError(err, 'link-install has no error')
    common.npm(['ls'], OPTS, function (err, c, out) {
      t.ifError(err)
      t.equal(c, 1)
      t.has(out, /foo@1.0.0/, 'link-install ok')
      t.end()
    })
  })
})

test('link-inside-install fails', function (t) {
  process.chdir(linkInstall)
  t.notOk(lstatSync(linkInside).isSymbolicLink(), 'directory for linked package is not a symlink to begin with')
  common.npm(['link', 'inside'], OPTS, function (err, code) {
    t.ifError(err, 'npm removed the linked package without error')
    t.notEqual(code, 0, 'link operation failed')
    t.notOk(lstatSync(linkInside).isSymbolicLink(), 'directory for linked package has not turned into a symlink')
    t.end()
  })
})

test('link-install the scoped package', function (t) {
  process.chdir(linkInstall)
  common.npm(['link', linkScoped], OPTS, function (err) {
    t.ifError(err, 'link-install has no error')
    common.npm(['ls'], OPTS, function (err, c, out) {
      t.ifError(err)
      t.equal(c, 1)
      t.has(out, /@scope[/]foo@1.0.0/, 'link-install ok')
      t.end()
    })
  })
})

test('cleanup', function (t) {
  process.chdir(osenv.tmpdir())
  common.npm(['rm', 'foo'], OPTS, function (err, code) {
    t.ifError(err, 'npm removed the linked package without error')
    t.equal(code, 0, 'cleanup foo in local ok')
    common.npm(['rm', '-g', 'foo'], OPTS, function (err, code) {
      t.ifError(err, 'npm removed the global package without error')
      t.equal(code, 0, 'cleanup foo in global ok')

      cleanup()
      t.end()
    })
  })
})

function cleanup () {
  rimraf.sync(linkRoot)
  rimraf.sync(link)
  rimraf.sync(linkScoped)
  rimraf.sync(linkInstall)
  rimraf.sync(linkInside)
}

function setup () {
  cleanup()
  mkdirp.sync(linkRoot)
  mkdirp.sync(link)
  writeFileSync(
    path.join(link, 'package.json'),
    JSON.stringify(readJSON, null, 2)
  )
  mkdirp.sync(linkScoped)
  writeFileSync(
    path.join(linkScoped, 'package.json'),
    JSON.stringify(readScopedJSON, null, 2)
  )
  mkdirp.sync(linkInstall)
  writeFileSync(
    path.join(linkInstall, 'package.json'),
    JSON.stringify(installJSON, null, 2)
  )
  mkdirp.sync(linkInside)
  writeFileSync(
    path.join(linkInside, 'package.json'),
    JSON.stringify(insideInstallJSON, null, 2)
  )
  writeFileSync(configPath, config)
}
