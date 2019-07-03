'use strict'
var test = require('tap').test
var path = require('path')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var fs = require('graceful-fs')
var common = require('../common-tap')

var base = common.pkg
var modA = path.resolve(base, 'modA')
var modB = path.resolve(base, 'modB')

var json = {
  'name': 'test-full-warning-messages',
  'version': '1.0.0',
  'description': 'abc',
  'repository': 'git://abc/',
  'license': 'ISC',
  'dependencies': {
    'modA': modA
  }
}

var modAJson = {
  'name': 'modA',
  'version': '1.0.0',
  'optionalDependencies': {
    'modB': modB
  }
}

var modBJson = {
  'name': 'modB',
  'version': '1.0.0',
  'os': ['nope'],
  'cpu': 'invalid'
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
  ;[modA, modB].forEach(function (mod) { mkdirp.sync(mod) })
  writeJson(base, json)
  writeJson(modA, modAJson)
  writeJson(modB, modBJson)
}

function cleanup () {
  rimraf.sync(base)
}

test('setup', function (t) {
  setup()
  t.end()
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

function notExists (t, filepath, msg) {
  try {
    fs.statSync(filepath)
    t.fail(msg, {found: 'exists', wanted: null, compare: 'fs.stat(' + filepath + ')'})
    return true
  } catch (ex) {
    t.pass(msg)
    return false
  }
}

test('tree-style', function (t) {
  common.npm(['install', '--json', '--loglevel=warn'], {cwd: base}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'result code')
    var result = JSON.parse(stdout)
    t.is(result.added.length, 1, 'only added one module')
    t.is(result.added[0].name, 'modA', 'modA got installed')
    t.is(result.warnings.length, 1, 'one warning')
    var stderrlines = stderr.trim().split(/\n/)
    t.is(stderrlines.length, 2, 'two lines of warnings')
    t.match(stderr, /SKIPPING OPTIONAL DEPENDENCY/, 'expected optional failure warning in stderr')
    t.match(stderr, /Unsupported platform/, 'reason for optional failure in stderr')
    t.match(result.warnings[0], /SKIPPING OPTIONAL DEPENDENCY/, 'expected optional failure warning in JSON')
    t.match(result.warnings[0], /Unsupported platform/, 'reason for optional failure in JSON')
    exists(t, modJoin(base, 'modA'), 'module A')
    notExists(t, modJoin(base, 'modB'), 'module B')
    t.done()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})
