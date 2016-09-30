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
var installed = path.join(example, 'node_modules', 'installed')
var installed_pkg = path.join(installed, 'package.json')

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
    'installed': '1.0'
  },
  devDependencies: {}
}

var example_shrinkwrap_json = {
  name: 'example',
  version: '1.0.0',
  dependencies: {
    installed: {
      version: '1.0.0'
    }
  }
}

var installed_pkg_json = {
  _id: 'installed@1.0.0',
  name: 'installed',
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
  writeJson(installed_pkg, installed_pkg_json)
  t.end()
})

test('install --save-dev leaves dev deps alone', function (t) {
  common.npm(['install', '--save-dev', 'file://' + installme], EXEC_OPTS, function (er, code, stdout, stderr) {
    t.ifError(er, "spawn didn't catch fire")
    t.is(code, 0, 'install completed ok')
    t.is(stderr, '', 'install completed without error output')
    var shrinkwrap = JSON.parse(fs.readFileSync(example_shrinkwrap))
    t.ok(shrinkwrap.dependencies.installed, "save-dev new install didn't remove dep")
    t.notOk(shrinkwrap.dependencies.installme, 'save-dev new install DID NOT add new dev dep')
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
