/* eslint-disable camelcase */
var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var test = require('tap').test

var common = require('../common-tap.js')

var base = common.pkg
var installme = path.join(base, 'installme')
var installme_pkg = path.join(installme, 'package.json')
var example = path.join(base, 'example')
var example_shrinkwrap = path.join(example, 'npm-shrinkwrap.json')
var example_pkg = path.join(example, 'package.json')
var installed = path.join(example, 'node_modules', 'installed')
var installed_pkg = path.join(installed, 'package.json')

// Ignore max listeners warnings until that gets fixed
var env = Object.keys(process.env).reduce((set, key) => {
  if (!set[key]) set[key] = process.env[key]
  return set
}, { NODE_NO_WARNINGS: '1' })

var EXEC_OPTS = { cwd: example, env: env }

var installme_pkg_json = {
  name: 'installme',
  version: '1.0.0',
  dependencies: {}
}

var example_pkg_json = {
  name: 'example',
  version: '1.0.0',
  dependencies: {},
  devDependencies: {
    'installed': '1.0'
  }
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
  writeJson(installme_pkg, installme_pkg_json)
  writeJson(example_pkg, example_pkg_json)
  writeJson(example_shrinkwrap, example_shrinkwrap_json)
  writeJson(installed_pkg, installed_pkg_json)
  t.end()
})

test('install --save leaves dev deps alone', t =>
  common.npm(['install', '--save', 'file://' + installme], EXEC_OPTS)
    .then(([code, stdout, stderr]) => {
      t.is(code, 0, 'install completed ok')
      t.is(stderr, '', 'install completed without error output')
      var shrinkwrap = JSON.parse(fs.readFileSync(example_shrinkwrap))
      t.ok(shrinkwrap.dependencies.installed, "save new install didn't remove dev dep")
      t.ok(shrinkwrap.dependencies.installme, 'save new install DID add new dep')
    }))
