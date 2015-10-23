var mkdirp = require('mkdirp')
var osenv = require('osenv')
var path = require('path')
var rimraf = require('rimraf')
var test = require('tap').test
var writeFileSync = require('fs').writeFileSync

var common = require('../common-tap.js')

var link = path.join(__dirname, 'rmlinked')
var linkDep = path.join(link, 'node_modules', 'baz')
var linkInstall = path.join(__dirname, 'rmlinked-install')
var linkRoot = path.join(__dirname, 'rmlinked-root')

var config = 'prefix = ' + linkRoot
var configPath = path.join(link, '_npmrc')

var OPTS = {
  env: {
    'npm_config_userconfig': configPath
  }
}

var linkedJSON = {
  name: 'foo',
  version: '1.0.0',
  description: '',
  main: 'index.js',
  scripts: {
    test: 'echo \"Error: no test specified\" && exit 1'
  },
  dependencies: {
    'baz': '1.0.0'
  },
  author: '',
  license: 'ISC'
}

var linkedDepJSON = {
  name: 'baz',
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
  dependencies: {
    'foo': '1.0.0'
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

test('creates global link', function (t) {
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

test('uninstall the global linked package', function (t) {
  process.chdir(osenv.tmpdir())
  common.npm(['uninstall', '-g', 'foo'], OPTS, function (err) {
    t.ifError(err, 'uninstall has no error')
    process.chdir(link)
    common.npm(['ls'], OPTS, function (err, c, out) {
      t.ifError(err)
      t.equal(c, 0)
      t.has(out, /baz@1.0.0/, "uninstall didn't remove dep")
      t.end()
    })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  process.chdir(osenv.tmpdir())
  try { rimraf.sync(linkRoot) } catch (e) { }
  try { rimraf.sync(linkDep) } catch (e) { }
  try { rimraf.sync(link) } catch (e) { }
  try { rimraf.sync(linkInstall) } catch (e) { }
}

function setup () {
  cleanup()
  mkdirp.sync(linkRoot)
  mkdirp.sync(link)
  writeFileSync(
    path.join(link, 'package.json'),
    JSON.stringify(linkedJSON, null, 2)
  )
  mkdirp.sync(linkDep)
  writeFileSync(
    path.join(linkDep, 'package.json'),
    JSON.stringify(linkedDepJSON, null, 2)
  )
  mkdirp.sync(linkInstall)
  writeFileSync(
    path.join(linkInstall, 'package.json'),
    JSON.stringify(installJSON, null, 2)
  )
  writeFileSync(configPath, config)
}
