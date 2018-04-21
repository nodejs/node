var common = require('../common-tap.js')
var test = require('tap').test
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var path = require('path')
var mr = require('npm-registry-mock')

var osenv = require('osenv')

var requireInject = require('require-inject')

var PKG_DIR = path.resolve(__dirname, 'update-examples')
var CACHE_DIR = path.resolve(PKG_DIR, 'cache')

// ** constant templates for mocks **
var DEFAULT_PKG = {
  'name': 'update-examples',
  'version': '1.2.3',
  'dependencies': {
    'dep1': '*'
  }
}

var DEP_PKG = {
  name: 'dep1',
  version: '1.1.1',
  _from: '^1.1.1'
}

var INSTALLED = {
  path: '/mock/root',
  realpath: '/mock/root',
  isLink: false,
  package: DEFAULT_PKG,
  children: [
    {
      path: '/mock/root/node_modules/dep1',
      realpath: '/mock/root/node_modules/dep1',
      isLink: false,
      package: DEP_PKG,
      children: []
    }
  ]
}

var DEP1_REGISTRY = { name: 'dep1',
  'dist-tags': { latest: '1.2.2' },
  versions: {
    '1.2.2': { version: '1.2.2' },
    '1.2.1': { version: '1.2.1' },
    '1.2.0': { version: '1.2.0' },
    '1.1.2': { version: '1.1.2' },
    '1.1.1': { version: '1.1.1' },
    '1.0.0': { version: '1.0.0' },
    '0.4.1': { version: '0.4.1' },
    '0.4.0': { version: '0.4.0' },
    '0.2.0': { version: '0.2.0' }
  }
}

var registryMocks = {
  'get': {
    '/dep1': [200, DEP1_REGISTRY]
  }
}

// ** dynamic mocks, cloned from templates and modified **
var mockServer
var mockDepJson = clone(DEP_PKG)
var mockInstalled = clone(INSTALLED)
var mockParentJson = clone(DEFAULT_PKG)

// target
var installAskedFor

function clone (a) {
  return extend({}, a)
}

function extend (a, b) {
  for (var key in b) {
    a[key] = b[key]
  }
  return a
}

function resetPackage (options) {
  rimraf.sync(CACHE_DIR)
  mkdirp.sync(CACHE_DIR)

  installAskedFor = undefined

  mockParentJson = clone(DEFAULT_PKG)
  mockInstalled = clone(INSTALLED)
  mockDepJson = clone(DEP_PKG)

  if (options.wanted) {
    mockParentJson.dependencies.dep1 = options.wanted
    mockInstalled.package.dependencies.dep1 = options.wanted
    mockDepJson._from = options.wanted
  }

  if (options.installed) {
    mockInstalled.package.dependencies.dep1 = options.installed
    mockInstalled.children[0].package.version = options.installed
    mockDepJson.version = options.installed
  }
}

function mockReadPackageTree (dir, cb) {
  cb(null, mockInstalled)
}

function mockReadJson (file, cb) {
  cb(null, file.match(/dep1/) ? mockDepJson : mockParentJson)
}

function mockCommand (npm, name, fn) {
  delete npm.commands[name]
  npm.commands[name] = fn
}

function mockInstaller (where, dryrun, what) {
  installAskedFor = what[0]
}
mockInstaller.prototype = {}
mockInstaller.prototype.run = function (cb) {
  return cb ? cb() : Promise.resolve()
}

var npm = requireInject.installGlobally('../../lib/npm.js', {
  'read-package-tree': mockReadPackageTree,
  'read-package-json': mockReadJson,
  '../../lib/install': {
    Installer: mockInstaller
  }
})

test('setup', function (t) {
  t.plan(5)
  process.chdir(osenv.tmpdir())
  mkdirp.sync(PKG_DIR)
  process.chdir(PKG_DIR)
  t.pass('made ' + PKG_DIR)

  resetPackage({})

  mr({ port: common.port, mocks: registryMocks }, function (er, server) {
    t.pass('mock registry active')
    npm.load({ cache: CACHE_DIR,
      registry: common.registry,
      cwd: PKG_DIR }, function (err) {
      t.ifError(err, 'started server')
      mockServer = server

      t.pass('npm.load complete')

      mockCommand(npm, 'install', function mockInstall (where, what, cb) {
        installAskedFor = what
        cb(null)
      })

      t.pass('mocks configured')
      t.end()
    })
  })
})

test('update caret dependency to latest', function (t) {
  resetPackage({ wanted: '^1.1.1' })

  npm.config.set('loglevel', 'silly')
  npm.commands.update([], function (err) {
    t.ifError(err)
    t.equal(installAskedFor, 'dep1@1.2.2', 'should want to install dep@1.2.2')
    t.end()
  })
})

test('update tilde dependency to latest', function (t) {
  resetPackage({ wanted: '~1.1.1' })

  npm.commands.update([], function (err) {
    t.ifError(err)
    t.equal(installAskedFor, 'dep1@1.1.2', 'should want to install dep@1.1.2')
    t.end()
  })
})

test('hold tilde dependency at wanted (#6441)', function (t) {
  resetPackage({ wanted: '~1.1.2', installed: '1.1.2' })

  npm.commands.update([], function (err) {
    t.ifError(err)
    t.notOk(installAskedFor, 'should not want to install anything')
    t.end()
  })
})

test('update old caret dependency with no newer', function (t) {
  resetPackage({ wanted: '^0.2.0', installed: '^0.2.0' })

  npm.commands.update([], function (err) {
    t.ifError(err)
    t.equal(installAskedFor, 'dep1@0.2.0', 'should want to install dep@0.2.0')
    t.end()
  })
})

test('update old caret dependency with newer', function (t) {
  resetPackage({ wanted: '^0.4.0', installed: '^0.4.0' })

  npm.commands.update([], function (err) {
    t.ifError(err)
    t.equal(installAskedFor, 'dep1@0.4.1', 'should want to install dep@0.4.1')
    t.end()
  })
})

test('cleanup', function (t) {
  mockServer.close()

  process.chdir(osenv.tmpdir())
  rimraf.sync(PKG_DIR)

  t.end()
})
