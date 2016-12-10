'use strict'
var test = require('tap').test
var fs = require('fs')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var path = require('path')
var common = require('../common-tap.js')

var testname = path.basename(__filename, '.js')
var testdir = path.resolve(__dirname, testname)
var testmod = path.resolve(testdir, 'top-test')

var bundleupdatesrc = path.resolve(testmod, 'bundle-update')
var bundleupdateNEW = path.resolve(bundleupdatesrc, 'NEW')
var bundleupdateNEWpostinstall = path.resolve(testdir, 'node_modules', 'top-test', 'node_modules', 'bundle-update', 'NEW')
var bundleupdatebad = path.resolve(testmod, 'node_modules', 'bundle-update')

var bundlekeepsrc = path.resolve(testmod, 'bundle-keep')
var bundlekeep = path.resolve(testmod, 'node_modules', 'bundle-keep')
var bundlekeepOLD = path.resolve(bundlekeep, 'OLD')
var bundlekeepOLDpostinstall = path.resolve(testdir, 'node_modules', 'top-test', 'node_modules', 'bundle-keep', 'OLD')

var bundledeepsrc = path.resolve(testmod, 'bundle-deep')
var bundledeep = path.resolve(testmod, 'node_modules', 'bundle-deep')
var bundledeepOLD = path.resolve(bundledeep, 'OLD')
var bundledeepOLDpostinstall = path.resolve(testdir, 'node_modules', 'top-test', 'node_modules', 'bundle-deep', 'OLD')

var bundledeepupdatesrc = path.resolve(testmod, 'bundle-deep-update')
var bundledeepupdate = path.resolve(bundledeep, 'node_modules', 'bundle-deep-update')
var bundledeepupdateNEW = path.resolve(bundledeepupdatesrc, 'NEW')
var bundledeepupdateNEWpostinstall = path.resolve(testdir, 'node_modules', 'top-test',
  'node_modules', 'bundle-deep', 'node_modules', 'bundle-deep-update', 'NEW')

var testjson = {
  dependencies: {'top-test': 'file:top-test/'}
}

var testmodjson = {
  name: 'top-test',
  version: '1.0.0',
  dependencies: {
    'bundle-update': bundleupdatesrc,
    'bundle-keep': bundlekeepsrc,
    'bundle-deep': bundledeepsrc
  },
  bundledDependencies: ['bundle-update', 'bundle-keep', 'bundle-deep']
}
var bundlejson = {
  name: 'bundle-update',
  version: '1.0.0'
}

var bundlekeepjson = {
  name: 'bundle-keep',
  version: '1.0.0',
  _requested: {
    rawSpec: bundlekeepsrc
  }
}
var bundledeepjson = {
  name: 'bundle-deep',
  version: '1.0.0',
  dependencies: {
    'bundle-deep-update': bundledeepupdatesrc
  },
  _requested: {
    rawSpec: bundledeepsrc
  }
}

var bundledeepupdatejson = {
  version: '1.0.0',
  name: 'bundle-deep-update'
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

  mkdirp.sync(bundledeepsrc)
  writepjs(bundledeepsrc, bundledeepjson)
  mkdirp.sync(bundledeep)
  writepjs(bundledeep, bundledeepjson)
  fs.writeFileSync(bundledeepOLD, '')

  mkdirp.sync(bundledeepupdatesrc)
  writepjs(bundledeepupdatesrc, bundledeepupdatejson)
  mkdirp.sync(bundledeepupdate)
  writepjs(bundledeepupdate, bundledeepupdatejson)
  fs.writeFileSync(bundledeepupdateNEW, '')
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
    t.plan(8)
    t.is(code, 0, 'npm itself completed ok')

    // This tests that after the install we have a freshly installed version
    // of `bundle-update` (in alignment with the package.json), instead of the
    // version that was bundled with `top-test`.
    // If npm doesn't do this, and selects the bundled version, things go very
    // wrong because npm thinks it has a different module (with different
    // metadata) installed in that location and will go off and try to do
    // _things_ to it.  Things like chmod in particular, which in turn results
    // in the dreaded ENOENT errors.
    t.like(stderr, new RegExp('npm WARN ' + testname), "didn't stomp on other warnings")
    t.like(stderr, /npm WARN.*bundle-update/, 'included update warning about bundled dep')
    t.like(stderr, /npm WARN.*bundle-deep-update/, 'included update warning about deeply bundled dep')
    fs.stat(bundleupdateNEWpostinstall, function (missing) {
      t.ok(!missing, 'package.json overrode bundle')
    })
    fs.stat(bundledeepupdateNEWpostinstall, function (missing) {
      t.ok(!missing, 'deep package.json overrode bundle')
    })
    // Relatedly, when upgrading, if a bundled module is replacing an existing
    // module we want to choose the bundled version, not the version we're replacing.
    fs.stat(bundlekeepOLDpostinstall, function (missing) {
      t.ok(!missing, 'no override when package.json matches')
    })
    fs.stat(bundledeepOLDpostinstall, function (missing) {
      t.ok(!missing, 'deep no override when package.json matches')
    })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})
