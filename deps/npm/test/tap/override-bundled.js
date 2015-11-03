'use strict'
var test = require('tap').test
var fs = require('fs')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var path = require('path')
var common = require('../common-tap.js')

var testdir = path.resolve(__dirname, path.basename(__filename, '.js'))
var testjson = {
  dependencies: {'top-test': 'file:top-test/'}
}

var testmod = path.resolve(testdir, 'top-test')
var testmodjson = {
  name: 'top-test',
  version: '1.0.0',
  dependencies: {
    'bundle-update': 'file:bundle-update/',
    'bundle-keep': 'file:bundle-keep/'
  },
  bundledDependencies: ['bundle-update', 'bundle-keep']
}

var bundleupdatesrc = path.resolve(testmod, 'bundle-update')
var bundleupdateNEW = path.resolve(bundleupdatesrc, 'NEW')
var bundleupdateNEWpostinstall = path.resolve(testdir, 'node_modules', 'top-test', 'node_modules', 'bundle-update', 'NEW')
var bundleupdatebad = path.resolve(testmod, 'node_modules', 'bundle-update')
var bundlekeepsrc = path.resolve(testmod, 'bundle-keep')
var bundlekeep = path.resolve(testmod, 'node_modules', 'bundle-keep')
var bundlekeepOLD = path.resolve(bundlekeep, 'OLD')
var bundlekeepOLDpostinstall = path.resolve(testdir, 'node_modules', 'top-test', 'node_modules', 'bundle-keep', 'OLD')
var bundlejson = {
  name: 'bundle-update',
  version: '1.0.0'
}
var bundlekeepjson = {
  name: 'bundle-keep',
  _requested: {
    rawSpec: 'file:bundle-keep/'
  }
}

function writepjs (dir, content) {
  fs.writeFileSync(path.join(dir, 'package.json'), JSON.stringify(content, null, 2))
}

function setup () {
  mkdirp.sync(testdir)
  writepjs(testdir, testjson)
  mkdirp.sync(testmod)
  writepjs(testmod, testmodjson)
  mkdirp.sync(bundleupdatesrc)
  writepjs(bundleupdatesrc, bundlejson)
  fs.writeFileSync(bundleupdateNEW, '')
  mkdirp.sync(bundleupdatebad)
  writepjs(bundleupdatebad, bundlejson)
  mkdirp.sync(bundlekeepsrc)
  writepjs(bundlekeepsrc, bundlekeepjson)
  mkdirp.sync(bundlekeep)
  writepjs(bundlekeep, bundlekeepjson)
  fs.writeFileSync(bundlekeepOLD, '')
}

function cleanup () {
  rimraf.sync(testdir)
}

test('setup', function (t) {
  cleanup()
  setup()
  t.end()
})

test('bundled', function (t) {
  common.npm(['install', '--loglevel=warn'], {cwd: testdir}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.plan(5)
    t.is(code, 0, 'npm itself completed ok')

    // This tests that after the install we have a freshly installed version
    // of `bundle-update` (in alignment with the package.json), instead of the
    // version that was bundled with `top-test`.
    // If npm doesn't do this, and selects the bundled version, things go very
    // wrong because npm thinks it has a different module (with different
    // metadata) installed in that location and will go off and try to do
    // _things_ to it.  Things like chmod in particular, which in turn results
    // in the dreaded ENOENT errors.
    t.like(stderr, /EPACKAGEJSON override-bundled/, "didn't stomp on other warnings")
    t.like(stderr, /EBUNDLEOVERRIDE/, 'included warning about bundled dep')
    fs.stat(bundleupdateNEWpostinstall, function (missing) {
      t.ok(!missing, 'package.json overrode bundle')
    })

    // Relatedly, when upgrading, if a bundled module is replacing an existing
    // module we want to choose the bundled version, not the version we're replacing.
    fs.stat(bundlekeepOLDpostinstall, function (missing) {
      t.ok(!missing, 'package.json overrode bundle')
    })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})
