var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var preferGlobalJson = {
  name: 'npm-test-preferglobal-dep',
  version: '0.0.0',
  preferGlobal: true
}

var dependenciesJson = {
  name: 'npm-test-preferglobal-dependency-check',
  version: '0.0.0',
  dependencies: {
    'npm-test-preferglobal-dep': 'file:../' + preferGlobalJson.name
  }
}

var devDependenciesJson = {
  name: 'npm-test-preferglobal-devDependency-check',
  version: '0.0.0',
  devDependencies: {
    'npm-test-preferglobal-dep': 'file:../' + preferGlobalJson.name
  }
}

var emptyPackage = {
  name: 'npm-test-preferglobal-empty-package',
  version: '0.0.0'
}

test('install a preferGlobal dependency without warning', function (t) {
  setup(dependenciesJson)
  common.npm([
    'install',
    '--loglevel=warn'
  ], {}, function (err, code, stdout, stderr) {
    t.ifError(err, 'packages were installed')
    t.notMatch(
      stderr,
      /WARN.*prefer global/,
      'install should not warn when dependency is preferGlobal')
    t.end()
  })
})

test('install a preferGlobal dependency without warning', function (t) {
  setup(devDependenciesJson)
  common.npm([
    'install',
    '--loglevel=warn'
  ], {}, function (err, code, stdout, stderr) {
    t.ifError(err, 'packages were installed')
    t.notMatch(
      stderr,
      /WARN.*prefer global/,
      'install should not warn when devDependency is preferGlobal')
    t.end()
  })
})

test('warn if a preferGlobal package is being installed direct', function (t) {
  setup(emptyPackage)
  common.npm([
    'install',
    'file:../' + preferGlobalJson.name,
    '--loglevel=warn'
  ], {}, function (err, code, stdout, stderr) {
    t.ifError(err, 'packages were installed')
    t.match(
      stderr,
      /WARN.*prefer global/,
      'install should warn when new package is preferGlobal')
    t.end()
  })
})

test('warn if a preferGlobal package is being saved', function (t) {
  setup(emptyPackage)
  common.npm([
    'install',
    'file:../' + preferGlobalJson.name,
    '--save',
    '--loglevel=warn'
  ], {}, function (err, code, stdout, stderr) {
    t.ifError(err, 'packages were installed')
    t.match(
      stderr,
      /WARN.*prefer global/,
      'install should warn when new package is preferGlobal')
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function setup (json) {
  cleanup()
  mkPkg(preferGlobalJson)
  process.chdir(mkPkg(json))
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  var pkgs = [preferGlobalJson,
              dependenciesJson,
              devDependenciesJson,
              emptyPackage]
  pkgs.forEach(function (json) {
    rimraf.sync(path.resolve(__dirname, json.name))
  })
}

function mkPkg (json) {
  var pkgPath = path.resolve(__dirname, json.name)
  mkdirp.sync(pkgPath)
  fs.writeFileSync(
    path.join(pkgPath, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  return pkgPath
}
