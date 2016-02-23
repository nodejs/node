var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var root = path.join(__dirname, 'install-save-local')
var pkg = path.join(root, 'package')

var EXEC_OPTS = { cwd: pkg }

var json = {
  name: 'install-save-local',
  version: '0.0.0'
}

var localDependency = {
  name: 'package-local-dependency',
  version: '0.0.0'
}

var localDevDependency = {
  name: 'package-local-dev-dependency',
  version: '0.0.0'
}

test('setup', function (t) {
  setup()
  t.end()
})

test('\'npm install --save ../local/path\' should save to package.json', function (t) {
  common.npm(
    [
      '--loglevel', 'silent',
      '--save',
      'install', '../package-local-dependency'
    ],
    EXEC_OPTS,
    function (err, code) {
      t.ifError(err, 'npm install ran without issue')
      t.notOk(code, 'npm install exited with code 0')

      var dependencyPackageJson = path.join(
        pkg, 'node_modules', 'package-local-dependency', 'package.json'
      )
      t.ok(JSON.parse(fs.readFileSync(dependencyPackageJson, 'utf8')))

      var pkgJson = JSON.parse(fs.readFileSync(pkg + '/package.json', 'utf8'))
      t.is(Object.keys(pkgJson.dependencies).length, 1, 'only one dep')
      t.ok(
        /file:.*?[/]package-local-dependency$/.test(pkgJson.dependencies['package-local-dependency']),
        'local package saved correctly'
      )
      t.end()
    }
  )
})

test('\'npm install --save-dev ../local/path\' should save to package.json', function (t) {
  setup()
  common.npm(
    [
      '--loglevel', 'silent',
      '--save-dev',
      'install', '../package-local-dev-dependency'
    ],
    EXEC_OPTS,
    function (err, code) {
      t.ifError(err, 'npm install ran without issue')
      t.notOk(code, 'npm install exited with code 0')

      var dependencyPackageJson = path.resolve(
        pkg, 'node_modules', 'package-local-dev-dependency', 'package.json'
      )
      t.ok(JSON.parse(fs.readFileSync(dependencyPackageJson, 'utf8')))

      var pkgJson = JSON.parse(fs.readFileSync(pkg + '/package.json', 'utf8'))
      t.is(Object.keys(pkgJson.devDependencies).length, 1, 'only one dep')
      t.ok(
        /file:.*?[/]package-local-dev-dependency$/.test(pkgJson.devDependencies['package-local-dev-dependency']),
        'local package saved correctly'
      )

      t.end()
    }
  )
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  process.chdir(osenv.tmpdir())
  process.chdir(__dirname)
  rimraf.sync(root)
}

function setup () {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )

  mkdirp.sync(path.join(root, 'package-local-dependency'))
  fs.writeFileSync(
    path.join(root, 'package-local-dependency', 'package.json'),
    JSON.stringify(localDependency, null, 2)
  )

  mkdirp.sync(path.join(root, 'package-local-dev-dependency'))
  fs.writeFileSync(
    path.join(root, 'package-local-dev-dependency', 'package.json'),
    JSON.stringify(localDevDependency, null, 2)
  )

  process.chdir(pkg)
}
