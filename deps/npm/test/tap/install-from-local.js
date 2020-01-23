var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var test = require('tap').test

var common = require('../common-tap')

var root = common.pkg
var pkg = path.join(root, 'package-with-local-paths')

var EXEC_OPTS = { cwd: pkg }

var localPaths = {
  name: 'package-with-local-paths',
  version: '0.0.0',
  dependencies: {
    'package-local-dependency': 'file:../package-local-dependency'
  },
  devDependencies: {
    'package-local-dev-dependency': 'file:../package-local-dev-dependency'
  }
}

var localDependency = {
  name: 'package-local-dependency',
  version: '0.0.0',
  description: 'Test for local installs'
}

var localDevDependency = {
  name: 'package-local-dev-dependency',
  version: '0.0.0',
  description: 'Test for local installs'
}

test('setup', function (t) {
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(localPaths, null, 2)
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
  t.end()
})

test('\'npm install\' should install local packages', function (t) {
  common.npm(
    [
      'install', '.'
    ],
    EXEC_OPTS,
    function (err, code) {
      t.ifError(err, 'error should not exist')
      t.notOk(code, 'npm install exited with code 0')
      var dependencyPackageJson = path.resolve(
        pkg,
        'node_modules/package-local-dependency/package.json'
      )
      t.ok(
        JSON.parse(fs.readFileSync(dependencyPackageJson, 'utf8')),
        'package with local dependency installed'
      )

      var devDependencyPackageJson = path.resolve(
        pkg, 'node_modules/package-local-dev-dependency/package.json'
      )
      t.ok(
        JSON.parse(fs.readFileSync(devDependencyPackageJson, 'utf8')),
        'package with local dev dependency installed'
      )

      t.end()
    }
  )
})
