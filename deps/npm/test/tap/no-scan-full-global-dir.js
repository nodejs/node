'use strict'
var fs = require('fs')
var path = require('path')
var test = require('tap').test
var requireInject = require('require-inject')
var osenv = require('osenv')
var npm = require('../../lib/npm.js')

var packages = {
  test: {package: {name: 'test'}, path: __dirname, children: ['abc', 'def', 'ghi', 'jkl']},
  abc: {package: {name: 'abc'}, path: path.join(__dirname, 'node_modules', 'abc')},
  def: {package: {name: 'def'}, path: path.join(__dirname, 'node_modules', 'def')},
  ghi: {package: {name: 'ghi'}, path: path.join(__dirname, 'node_modules', 'ghi')},
  jkl: {package: {name: 'jkl'}, path: path.join(__dirname, 'node_modules', 'jkl')}
}
var dirs = {}
var files = {}
Object.keys(packages).forEach(function (name) {
  dirs[path.join(packages[name].path, 'node_modules')] = packages[name].children || []
  files[path.join(packages[name].path, 'package.json')] = packages[name].package
})

process.chdir(osenv.tmpdir())

var mockReaddir = function (name, cb) {
  if (dirs[name]) return cb(null, dirs[name])
  var er = new Error('No such mock: ' + name)
  er.code = 'ENOENT'
  cb(er)
}
var mockReadPackageJson = function (file, cb) {
  if (files[file]) return cb(null, files[file])
  var er = new Error('No such mock: ' + file)
  er.code = 'ENOENT'
  cb(er)
}
var mockFs = Object.create(fs)
mockFs.realpath = function (dir, cb) {
  return cb(null, dir)
}

test('setup', function (t) {
  npm.load(function () {
    t.pass('npm loaded')
    t.end()
  })
})

test('installer', function (t) {
  t.plan(1)
  var installer = requireInject('../../lib/install.js', {
    'fs': mockFs,
    'readdir-scoped-modules': mockReaddir,
    'read-package-json': mockReadPackageJson,
    'mkdirp': function (path, cb) { cb() }
  })

  var Installer = installer.Installer
  class TestInstaller extends Installer {
    constructor (where, dryrun, args) {
      super(where, dryrun, args)
      this.global = true
    }
    loadArgMetadata (cb) {
      this.args = this.args.map(function (arg) { return {name: arg} })
      cb()
    }
  }

  var inst = new TestInstaller(__dirname, false, ['def', 'abc'])
  inst.loadCurrentTree(function () {
    var kids = inst.currentTree.children.map(function (child) { return child.package.name })
    t.isDeeply(kids, ['abc', 'def'])
    t.end()
  })
})

test('uninstaller', function (t) {
  t.plan(1)
  var uninstaller = requireInject('../../lib/uninstall.js', {
    'fs': mockFs,
    'readdir-scoped-modules': mockReaddir,
    'read-package-json': mockReadPackageJson,
    'mkdirp': function (path, cb) { cb() }
  })

  var Uninstaller = uninstaller.Uninstaller
  class TestUninstaller extends Uninstaller {
    constructor (where, dryrun, args) {
      super(where, dryrun, args)
      this.global = true
    }
  }

  var uninst = new TestUninstaller(__dirname, false, ['ghi', 'jkl'])
  uninst.loadCurrentTree(function () {
    var kids = uninst.currentTree.children.map(function (child) { return child.package.name })
    t.isDeeply(kids, ['ghi', 'jkl'])
    t.end()
  })
})
