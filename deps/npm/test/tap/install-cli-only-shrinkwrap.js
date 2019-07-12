var fs = require('graceful-fs')
var path = require('path')
var existsSync = fs.existsSync || path.existsSync

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = common.pkg

var EXEC_OPTS = { cwd: pkg }

var json = {
  name: 'install-cli-only-shrinkwrap',
  description: 'fixture',
  version: '0.0.0',
  dependencies: {
    dependency: 'file:./dependency'
  },
  devDependencies: {
    'dev-dependency': 'file:./dev-dependency'
  }
}

var shrinkwrap = {
  name: 'install-cli-only-shrinkwrap',
  description: 'fixture',
  version: '0.0.0',
  lockfileVersion: 1,
  requires: true,
  dependencies: {
    dependency: {
      version: 'file:./dependency'
    },
    'dev-dependency': {
      version: 'file:./dev-dependency',
      dev: true
    }
  }
}

var dependency = {
  name: 'dependency',
  description: 'fixture',
  version: '0.0.0'
}

var devDependency = {
  name: 'dev-dependency',
  description: 'fixture',
  version: '0.0.0'
}

test('setup', function (t) {
  cleanup()
  setup()
  t.pass('setup ran')
  t.end()
})

test('\'npm install --only=development\' should only install devDependencies', function (t) {
  common.npm(['install', '--only=development'], EXEC_OPTS, function (err, code, stderr, stdout) {
    if (err) throw err
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    t.is(code, 0, 'npm install did not raise error code')
    t.ok(
      existsSync(
        path.resolve(pkg, 'node_modules/dev-dependency/package.json')
      ),
      'devDependency was installed'
    )
    t.notOk(
      existsSync(path.resolve(pkg, 'node_modules/dependency/package.json')),
      'dependency was NOT installed'
    )
    t.end()
  })
})

test('\'npm install --only=production\' should only install dependencies', function (t) {
  cleanup()
  setup()
  common.npm(['install', '--only=production'], EXEC_OPTS, function (err, code, stdout, stderr) {
    if (err) throw err
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    t.is(code, 0, 'npm install did not raise error code')
    t.ok(
      existsSync(
        path.resolve(pkg, 'node_modules/dependency/package.json')
      ),
      'dependency was installed'
    )
    t.notOk(
      existsSync(path.resolve(pkg, 'node_modules/dev-dependency/package.json')),
      'devDependency was NOT installed'
    )
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.pass('cleaned up')
  t.end()
})

function setup () {
  mkdirp.sync(path.join(pkg, 'dependency'))
  fs.writeFileSync(
    path.join(pkg, 'dependency', 'package.json'),
    JSON.stringify(dependency, null, 2)
  )

  mkdirp.sync(path.join(pkg, 'dev-dependency'))
  fs.writeFileSync(
    path.join(pkg, 'dev-dependency', 'package.json'),
    JSON.stringify(devDependency, null, 2)
  )

  mkdirp.sync(path.join(pkg, 'node_modules'))
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  fs.writeFileSync(
    path.join(pkg, 'npm-shrinkwrap.json'),
    JSON.stringify(shrinkwrap, null, 2)
  )
  process.chdir(pkg)
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
