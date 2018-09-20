'use strict'
var Bluebird = require('bluebird')
var test = require('tap').test
var path = require('path')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var fs = require('graceful-fs')
var common = require('../common-tap')

var base = path.resolve(__dirname, path.basename(__filename, '.js'))
var modA = path.resolve(base, 'modA')
var modB = path.resolve(base, 'modB')
var modC = path.resolve(base, 'modC')
var testNormal = path.resolve(base, 'testNormal')
var testGlobal = path.resolve(base, 'testGlobal')
var testLegacy = path.resolve(base, 'testLegacy')

var json = {
  'name': 'test-tree-style',
  'version': '1.0.0',
  'dependencies': {
    'modA': modA + '-1.0.0.tgz'
  }
}

var modAJson = {
  'name': 'modA',
  'version': '1.0.0',
  'dependencies': {
    'modB': modB + '-1.0.0.tgz'
  }
}

var modBJson = {
  'name': 'modB',
  'version': '1.0.0',
  'dependencies': {
    'modC': modC + '-1.0.0.tgz'
  }
}

var modCJson = {
  'name': 'modC',
  'version': '1.0.0'
}

function modJoin () {
  var modules = Array.prototype.slice.call(arguments)
  return modules.reduce(function (a, b) {
    return path.resolve(a, 'node_modules', b)
  })
}

function writeJson (mod, data) {
  fs.writeFileSync(path.resolve(mod, 'package.json'), JSON.stringify(data))
}

function setup () {
  cleanup()
  ;[modA, modB, modC, testNormal, testGlobal, testLegacy].forEach(function (mod) {
    mkdirp.sync(mod)
  })
  writeJson(modA, modAJson)
  writeJson(modB, modBJson)
  writeJson(modC, modCJson)
  ;[testNormal, testGlobal, testLegacy].forEach(function (mod) { writeJson(mod, json) })
}

function cleanup () {
  rimraf.sync(base)
}

test('setup', function (t) {
  setup()
  return Bluebird.try(() => {
    return common.npm(['pack', 'file:modC'], {cwd: base})
  }).spread((code) => {
    t.is(code, 0, 'pack modC')
    return common.npm(['pack', 'file:modB'], {cwd: base})
  }).spread((code) => {
    t.is(code, 0, 'pack modB')
    return common.npm(['pack', 'file:modA'], {cwd: base})
  }).spread((code) => {
    t.is(code, 0, 'pack modA')
  })
})

function exists (t, filepath, msg) {
  try {
    fs.statSync(filepath)
    t.pass(msg)
    return true
  } catch (ex) {
    t.fail(msg, {found: null, wanted: 'exists', compare: 'fs.stat(' + filepath + ')'})
    return false
  }
}

test('tree-style', function (t) {
  t.plan(12)
  common.npm(['install'], {cwd: testNormal}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'normal install; result code')
    t.is(stderr, '', 'normal install; no errors')
    exists(t, modJoin(testNormal, 'modA'), 'normal install; module A')
    exists(t, modJoin(testNormal, 'modB'), 'normal install; module B')
    exists(t, modJoin(testNormal, 'modC'), 'normal install; module C')
  })
  common.npm(['install', '--global-style'], {cwd: testGlobal}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'global-style install; result code')
    t.is(stderr, '', 'global-style install; no errors')
    exists(t, modJoin(testGlobal, 'modA', 'modB'), 'global-style install; module B')
    exists(t, modJoin(testGlobal, 'modA', 'modC'), 'global-style install; module C')
  })
  common.npm(['install', '--legacy-bundling'], {cwd: testLegacy}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'legacy-bundling install; result code')
    t.is(stderr, '', 'legacy-bundling install; no errors')
    exists(t, modJoin(testLegacy, 'modA', 'modB', 'modC'), 'legacy-bundling install; module C')
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})
