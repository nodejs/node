var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var base = path.resolve(__dirname, path.basename(__filename, '.js'))
var installme = path.join(base, 'installme')
var installme_pkg = path.join(installme, 'package.json')
var example = path.join(base, 'example')
var example_shrinkwrap = path.join(example, 'npm-shrinkwrap.json')
var example_pkg = path.join(example, 'package.json')
var installed_prod = path.join(example, 'node_modules', 'installed-prod')
var installed_prod_pkg = path.join(installed_prod, 'package.json')
var installed_dev = path.join(example, 'node_modules', 'installed-dev')
var installed_dev_pkg = path.join(installed_dev, 'package.json')

var EXEC_OPTS = { cwd: example }

var installme_pkg_json = {
  name: 'installme',
  version: '1.0.0',
  dependencies: {}
}

var example_pkg_json = {
  name: 'example',
  version: '1.0.0',
  dependencies: {
    'installed-prod': '1.0'
  },
  devDependencies: {
    'installed-dev': '1.0'
  }
}

var example_shrinkwrap_json = {
  name: 'example',
  version: '1.0.0',
  dependencies: {
    'installed-prod': {
      version: '1.0.0'
    },
    'installed-dev': {
      version: '1.0.0'
    }
  }
}

var installed_prod_pkg_json = {
  _id: 'installed-prod@1.0.0',
  _integrity: 'sha1-deadbeef',
  _resolved: 'foo',
  name: 'installed-prod',
  version: '1.0.0'
}

var installed_dev_pkg_json = {
  _id: 'installed-dev@1.0.0',
  _integrity: 'sha1-deadbeef',
  _resolved: 'foo',
  name: 'installed-dev',
  version: '1.0.0'
}

function writeJson (filename, obj) {
  mkdirp.sync(path.dirname(filename))
  fs.writeFileSync(filename, JSON.stringify(obj, null, 2))
}

test('setup', function (t) {
  cleanup()
  writeJson(installme_pkg, installme_pkg_json)
  writeJson(example_pkg, example_pkg_json)
  writeJson(example_shrinkwrap, example_shrinkwrap_json)
  writeJson(installed_prod_pkg, installed_prod_pkg_json)
  writeJson(installed_dev_pkg, installed_dev_pkg_json)
  t.end()
})

test('install --save-dev leaves prod deps alone', function (t) {
  common.npm(['install', '--save-dev', 'file://' + installme], EXEC_OPTS, function (er, code, stdout, stderr) {
    t.ifError(er, "spawn didn't catch fire")
    t.is(code, 0, 'install completed ok')
    t.is(stderr, '', 'install completed without error output')
    var shrinkwrap = JSON.parse(fs.readFileSync(example_shrinkwrap))
    t.ok(shrinkwrap.dependencies['installed-prod'], "save-dev new install didn't remove prod dep")
    t.ok(shrinkwrap.dependencies['installed-dev'], "save-dev new install didn't remove dev dep")
    t.ok(shrinkwrap.dependencies.installme, 'save-dev new install DID add new dev dep')
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(base)
}
