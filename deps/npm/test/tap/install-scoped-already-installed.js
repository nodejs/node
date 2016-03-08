var fs = require('graceful-fs')
var path = require('path')
var existsSync = fs.existsSync || path.existsSync

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap')

var root = path.join(__dirname, 'install-scoped-already-installed')
var pkg = path.join(root, 'package-with-scoped-paths')
var modules = path.join(pkg, 'node_modules')

var EXEC_OPTS = { cwd: pkg }

var scopedPaths = {
  name: 'package-with-scoped-paths',
  version: '0.0.0',
  dependencies: {
    'package-local-dependency': 'file:../package-local-dependency',
    '@scoped/package-scoped-dependency': 'file:../package-scoped-dependency'
  }
}

var localDependency = {
  name: 'package-local-dependency',
  version: '0.0.0',
  description: 'Test for local installs'
}

var scopedDependency = {
  name: '@scoped/package',
  version: '0.0.0',
  description: 'Test for local installs'
}

test('setup', function (t) {
  rimraf.sync(root)
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(scopedPaths, null, 2)
  )

  mkdirp.sync(path.join(root, 'package-local-dependency'))
  fs.writeFileSync(
    path.join(root, 'package-local-dependency', 'package.json'),
    JSON.stringify(localDependency, null, 2)
  )

  mkdirp.sync(path.join(root, 'package-scoped-dependency'))
  fs.writeFileSync(
    path.join(root, 'package-scoped-dependency', 'package.json'),
    JSON.stringify(scopedDependency, null, 2)
  )

  process.chdir(pkg)
  t.end()
})

test('installing already installed local scoped package', function (t) {
  common.npm(
    [
      '--loglevel', 'silent',
      'install'
    ],
    EXEC_OPTS,
    function (err, code, stdout) {
      var installed = parseNpmInstallOutput(stdout)
      t.ifError(err, 'install ran to completion without error')
      t.notOk(code, 'npm install exited with code 0')

      t.ok(
        existsSync(path.join(modules, '@scoped', 'package', 'package.json')),
        'package installed'
      )
      t.ok(
        contains(installed, 'node_modules/@scoped/package'),
        'installed @scoped/package'
      )
      t.ok(
        contains(installed, 'node_modules/package-local-dependency'),
        'installed package-local-dependency'
      )

      common.npm(
        [
          '--loglevel', 'silent',
          'install'
        ],
        EXEC_OPTS,
        function (err, code, stdout) {
          t.ifError(err, 'install ran to completion without error')
          t.notOk(code, 'npm install raised no error code')

          installed = parseNpmInstallOutput(stdout)

          t.ok(
            existsSync(path.join(modules, '@scoped', 'package', 'package.json')),
            'package installed'
          )

          t.notOk(
            contains(installed, 'node_modules/@scoped/package'),
            'did not reinstall @scoped/package'
          )
          t.notOk(
            contains(installed, 'node_modules/package-local-dependency'),
            'did not reinstall package-local-dependency'
          )
          t.end()
        }
      )
    }
  )
})

test('cleanup', function (t) {
  process.chdir(osenv.tmpdir())
  rimraf.sync(root)
  t.end()
})

function contains (list, element) {
  for (var i = 0; i < list.length; ++i) {
    if (list[i] === element) {
      return true
    }
  }
  return false
}

function parseNpmInstallOutput (stdout) {
  return stdout.trim().split(/\n\n|\s+/)
}
