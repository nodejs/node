var test = require('tap').test
var path = require('path')
var fs = require('fs')
var osenv = require('osenv')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var common = require('../common-tap.js')

var PKG_DIR = path.resolve(__dirname, 'shrinkwrap-local-dependency')
var CACHE_DIR = path.resolve(PKG_DIR, 'cache')
var DEP_DIR = path.resolve(PKG_DIR, 'dep')

var desired = {
  'name': 'npm-test-shrinkwrap-local-dependency',
  'version': '0.0.0',
  'dependencies': {
    'npm-test-shrinkwrap-local-dependency-dep': {
      'version': '0.0.0',
      'from': 'dep',
      'resolved': 'file:dep'
    }
  }
}

var root = {
  'author': 'Thomas Torp',
  'name': 'npm-test-shrinkwrap-local-dependency',
  'version': '0.0.0',
  'dependencies': {
    'npm-test-shrinkwrap-local-dependency-dep': 'file:./dep'
  }
}

var dependency = {
  'author': 'Thomas Torp',
  'name': 'npm-test-shrinkwrap-local-dependency-dep',
  'version': '0.0.0'
}

test('shrinkwrap uses resolved with file: on local deps', function (t) {
  setup()

  common.npm(
    ['--cache=' + CACHE_DIR, '--loglevel=silent', 'install', '.'],
    {},
    function (err, code) {
      t.ifError(err, 'npm install worked')
      t.equal(code, 0, 'npm exited normally')

      common.npm(
        ['--cache=' + CACHE_DIR, '--loglevel=silent', 'shrinkwrap'],
        {},
        function (err, code) {
          t.ifError(err, 'npm shrinkwrap worked')
          t.equal(code, 0, 'npm exited normally')

          fs.readFile('npm-shrinkwrap.json', { encoding: 'utf8' }, function (err, data) {
            t.ifError(err, 'read file correctly')
            t.deepEqual(JSON.parse(data), desired, 'shrinkwrap looks correct')

            t.end()
          })
        }
      )
    }
  )
})

test("'npm install' should install local packages from shrinkwrap", function (t) {
  cleanNodeModules()

  common.npm(
    ['--cache=' + CACHE_DIR, '--loglevel=silent', 'install', '.'],
    {},
    function (err, code) {
      t.ifError(err, 'install ran correctly')
      t.notOk(code, 'npm install exited with code 0')
      var dependencyPackageJson = path.resolve(
        PKG_DIR,
        'node_modules/npm-test-shrinkwrap-local-dependency-dep/package.json'
      )
      t.ok(
        JSON.parse(fs.readFileSync(dependencyPackageJson, 'utf8')),
        'package with local dependency installed from shrinkwrap'
      )

      t.end()
    }
  )
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function setup () {
  cleanup()
  mkdirp.sync(PKG_DIR)
  mkdirp.sync(CACHE_DIR)
  mkdirp.sync(DEP_DIR)
  fs.writeFileSync(
    path.resolve(PKG_DIR, 'package.json'),
    JSON.stringify(root, null, 2)
  )
  fs.writeFileSync(
    path.resolve(DEP_DIR, 'package.json'),
    JSON.stringify(dependency, null, 2)
  )
  process.chdir(PKG_DIR)
}

function cleanNodeModules () {
  rimraf.sync(path.resolve(PKG_DIR, 'node_modules'))
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  cleanNodeModules()
  rimraf.sync(PKG_DIR)
}
