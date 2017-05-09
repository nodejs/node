'use strict'
var Bluebird = require('bluebird')
var test = require('tap').test
var fs = require('fs')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var path = require('path')
var common = require('../common-tap.js')

var testname = path.basename(__filename, '.js')
var testdir = path.resolve(__dirname, testname)
var testmod = path.resolve(testdir, 'top-test')
var testtgz = testmod + '-1.0.0.tgz'

var bundleupdatesrc = path.resolve(testmod, 'bundle-update')
var bundleupdatetgz = bundleupdatesrc + '-1.0.0.tgz'
var bundleupdateNEW = path.resolve(bundleupdatesrc, 'NEW')
var bundleupdateNEWpostinstall = path.resolve(testdir, 'node_modules', 'top-test', 'node_modules', 'bundle-update', 'NEW')
var bundleupdatebad = path.resolve(testmod, 'node_modules', 'bundle-update')

var bundlekeepsrc = path.resolve(testmod, 'bundle-keep')
var bundlekeeptgz = bundlekeepsrc + '-1.0.0.tgz'
var bundlekeep = path.resolve(testmod, 'node_modules', 'bundle-keep')
var bundlekeepOLD = path.resolve(bundlekeep, 'OLD')
var bundlekeepOLDpostinstall = path.resolve(testdir, 'node_modules', 'top-test', 'node_modules', 'bundle-keep', 'OLD')

var bundledeepsrc = path.resolve(testmod, 'bundle-deep')
var bundledeeptgz = bundledeepsrc + '-1.0.0.tgz'
var bundledeep = path.resolve(testmod, 'node_modules', 'bundle-deep')
var bundledeepOLD = path.resolve(bundledeep, 'OLD')
var bundledeepOLDpostinstall = path.resolve(testdir, 'node_modules', 'top-test', 'node_modules', 'bundle-deep', 'OLD')

var bundledeepupdatesrc = path.resolve(testmod, 'bundle-deep-update')
var bundledeepupdatetgz = bundledeepupdatesrc + '-1.0.0.tgz'
var bundledeepupdate = path.resolve(bundledeep, 'node_modules', 'bundle-deep-update')
var bundledeepupdateNEW = path.resolve(bundledeepupdatesrc, 'NEW')
var bundledeepupdateNEWpostinstall = path.resolve(testdir, 'node_modules', 'top-test',
  'node_modules', 'bundle-deep', 'node_modules', 'bundle-deep-update', 'NEW')

var testjson = {
  dependencies: {'top-test': 'file:' + testtgz}
}

var testmodjson = {
  name: 'top-test',
  version: '1.0.0',
  dependencies: {
    'bundle-update': bundleupdatetgz,
    'bundle-keep': bundlekeeptgz,
    'bundle-deep': bundledeeptgz
  },
  bundledDependencies: ['bundle-update', 'bundle-keep', 'bundle-deep'],
  files: ['OLD', 'NEW']
}
var bundlejson = {
  name: 'bundle-update',
  version: '1.0.0',
  files: ['OLD', 'NEW']

}

var bundlekeepjson = {
  name: 'bundle-keep',
  version: '1.0.0',
  _requested: {
    rawSpec: bundlekeeptgz
  },
  files: ['OLD', 'NEW']

}
var bundledeepjson = {
  name: 'bundle-deep',
  version: '1.0.0',
  dependencies: {
    'bundle-deep-update': bundledeepupdatetgz
  },
  _requested: {
    rawSpec: bundledeeptgz
  },
  files: ['OLD', 'NEW']

}

var bundledeepupdatejson = {
  version: '1.0.0',
  name: 'bundle-deep-update',
  files: ['OLD', 'NEW']

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
  return Bluebird.all([
    common.npm(['pack', bundleupdatesrc], {cwd: path.dirname(bundleupdatetgz), stdio: 'inherit'}),
    common.npm(['pack', bundlekeepsrc], {cwd: path.dirname(bundlekeeptgz), stdio: 'inherit'}),
    common.npm(['pack', bundledeepupdatesrc], {cwd: path.dirname(bundledeepupdatetgz), stdio: 'inherit'})
  ]).map((result) => result[0]).spread((bundleupdate, bundlekeep, bundledeepupdate) => {
    t.is(bundleupdate, 0, 'bundleupdate')
    t.is(bundlekeep, 0, 'bundlekeep')
    t.is(bundledeepupdate, 0, 'bundledeepupdate')
  }).then(() => {
    return common.npm(['pack', bundledeepsrc], {cwd: path.dirname(bundledeeptgz), stdio: 'inherit'})
  }).spread((code) => {
    t.is(code, 0, 'bundledeep')
    return common.npm(['pack', testmod], {cwd: path.dirname(testtgz), stdio: 'inherit'})
  }).spread((code) => {
    t.is(code, 0, 'testmod')
  })
})

test('bundled', function (t) {
  common.npm(['install', '--loglevel=verbose'], {cwd: testdir}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.plan(9)
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
    t.like(stderr, /npm verb.*bundle-update/, 'included update warning about bundled dep')
    t.like(stderr, /npm verb.*bundle-deep-update/, 'included update warning about deeply bundled dep')
    t.like(stderr, /npm WARN top-test@1\.0\.0 had bundled packages that do not match/, 'single grouped warning')
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
