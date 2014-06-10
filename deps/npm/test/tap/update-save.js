var common = require("../common-tap.js")
var test = require("tap").test
var npm = require("../../")
var mkdirp = require("mkdirp")
var rimraf = require("rimraf")
var fs = require('fs')
var path = require('path')
var mr = require("npm-registry-mock")

var PKG_DIR = path.resolve(__dirname, "update-save")
var PKG = path.resolve(PKG_DIR, "package.json")
var CACHE_DIR = path.resolve(PKG_DIR, "cache")
var MODULES_DIR = path.resolve(PKG_DIR, "node_modules")

var EXEC_OPTS = {
  cwd: PKG_DIR,
  stdio: 'ignore',
  env: {
    npm_config_registry: common.registry,
    npm_config_loglevel: 'verbose'
  }
}

var DEFAULT_PKG = {
  "name": "update-save-example",
  "version": "1.2.3",
  "dependencies": {
    "mkdirp": "~0.3.0"
  },
  "devDependencies": {
    "underscore": "~1.3.1"
  }
}

var s = undefined // mock server reference

test('setup', function (t) {
  resetPackage()

  mr(common.port, function (server) {
    npm.load({cache: CACHE_DIR, registry: common.registry}, function (err) {
      t.ifError(err)
      s = server
      t.end()
    })
  })
})

test("update regular dependencies only", function (t) {
  resetPackage()

  common.npm(['update', '--save'], EXEC_OPTS, function (err, code) {
    t.ifError(err)
    t.equal(code, 0)

    var pkgdata = JSON.parse(fs.readFileSync(PKG, 'utf8'))
    t.deepEqual(pkgdata.dependencies, {mkdirp: '^0.3.5'}, 'only dependencies updated')
    t.deepEqual(pkgdata.devDependencies, DEFAULT_PKG.devDependencies, 'dev dependencies should be untouched')
    t.deepEqual(pkgdata.optionalDependencies, DEFAULT_PKG.optionalDependencies, 'optional dependencies should be untouched')
    t.end()
  })
})

test("update devDependencies only", function (t) {
  resetPackage()

  common.npm(['update', '--save-dev'], EXEC_OPTS, function (err, code, stdout, stderr) {
    t.ifError(err)
    t.equal(code, 0)

    var pkgdata = JSON.parse(fs.readFileSync(PKG, 'utf8'))
    t.deepEqual(pkgdata.dependencies, DEFAULT_PKG.dependencies, 'dependencies should be untouched')
    t.deepEqual(pkgdata.devDependencies, {underscore: '^1.3.3'}, 'dev dependencies should be updated')
    t.deepEqual(pkgdata.optionalDependencies, DEFAULT_PKG.optionalDependencies, 'optional dependencies should be untouched')
    t.end()
  })
})

test("update optionalDependencies only", function (t) {
  resetPackage({
    "optionalDependencies": {
      "underscore": "~1.3.1"
    }
  })

  common.npm(['update', '--save-optional'], EXEC_OPTS, function (err, code) {
    t.ifError(err)
    t.equal(code, 0)

    var pkgdata = JSON.parse(fs.readFileSync(PKG, 'utf8'))
    t.deepEqual(pkgdata.dependencies, DEFAULT_PKG.dependencies, 'dependencies should be untouched')
    t.deepEqual(pkgdata.devDependencies, DEFAULT_PKG.devDependencies, 'dev dependencies should be untouched')
    t.deepEqual(pkgdata.optionalDependencies, {underscore: '^1.3.3'}, 'optional dependencies should be updated')
    t.end()
  })
})

test("optionalDependencies are merged into dependencies during --save", function (t) {
  var pkg = resetPackage({
    "optionalDependencies": {
      "underscore": "~1.3.1"
    }
  })

  common.npm(['update', '--save'], EXEC_OPTS, function (err, code) {
    t.ifError(err)
    t.equal(code, 0)

    var pkgdata = JSON.parse(fs.readFileSync(PKG, 'utf8'))
    t.deepEqual(pkgdata.dependencies, {mkdirp: '^0.3.5'}, 'dependencies should not include optional dependencies')
    t.deepEqual(pkgdata.devDependencies, pkg.devDependencies, 'dev dependencies should be untouched')
    t.deepEqual(pkgdata.optionalDependencies, pkg.optionalDependencies, 'optional dependencies should be untouched')
    t.end()
  })
})

test("semver prefix is replaced with configured save-prefix", function (t) {
  resetPackage()

  common.npm(['update', '--save', '--save-prefix', '~'], EXEC_OPTS, function (err, code) {
    t.ifError(err)
    t.equal(code, 0)

    var pkgdata = JSON.parse(fs.readFileSync(PKG, 'utf8'))
    t.deepEqual(pkgdata.dependencies, {
      mkdirp: '~0.3.5'
    }, 'dependencies should be updated')
    t.deepEqual(pkgdata.devDependencies, DEFAULT_PKG.devDependencies, 'dev dependencies should be untouched')
    t.deepEqual(pkgdata.optionalDependencies, DEFAULT_PKG.optionalDependencies, 'optional dependencies should be updated')
    t.end()
  })
})

function resetPackage(extendWith) {
  rimraf.sync(CACHE_DIR)
  rimraf.sync(MODULES_DIR)
  mkdirp.sync(CACHE_DIR)
  var pkg = clone(DEFAULT_PKG)
  extend(pkg, extendWith)
  for (key in extend) { pkg[key] = extend[key]}
  fs.writeFileSync(PKG, JSON.stringify(pkg, null, 2), 'ascii')
  return pkg
}

test("cleanup", function (t) {
  s.close()
  resetPackage() // restore package.json
  rimraf.sync(CACHE_DIR)
  rimraf.sync(MODULES_DIR)
  t.end()
})

function clone(a) {
  return extend({}, a)
}

function extend(a, b) {
  for (key in b) { a[key] = b[key]}
  return a
}
