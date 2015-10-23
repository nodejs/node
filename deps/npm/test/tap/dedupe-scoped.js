var fs = require('fs')
var join = require('path').join

var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')
var pkg = join(__dirname, 'dedupe-scoped')
var modules = join(pkg, 'node_modules')

var EXEC_OPTS = { cwd: pkg }

var prolog = 'dedupe@0.0.0 ' + pkg
var body = function () {/*
├─┬ first@1.0.0
│ └── @scope/shared@2.1.6
└─┬ second@2.0.0
  └── @scope/shared@2.1.6
*/}.toString().split('\n').slice(1, -1)

var deduper = {
  'name': 'dedupe',
  'version': '0.0.0',
  'dependencies': {
    'first': '1.0.0',
    'second': '2.0.0'
  }
}

var first = {
  'name': 'first',
  'version': '1.0.0',
  'dependencies': {
    'firstUnique': '0.6.0',
    '@scope/shared': '2.1.6'
  }
}

var second = {
  'name': 'second',
  'version': '2.0.0',
  'dependencies': {
    'secondUnique': '1.2.0',
    '@scope/shared': '2.1.6'
  }
}

var shared = {
  'name': '@scope/shared',
  'version': '2.1.6'
}

var firstUnique = {
  'name': 'firstUnique',
  'version': '0.6.0'
}

var secondUnique = {
  'name': 'secondUnique',
  'version': '1.2.0'
}


test('setup', function (t) {
  setup()
  t.end()
})

// we like the cars
function ltrimm (l) { return l.trim() }

test('dedupe finds the common scoped modules and moves it up one level', function (t) {
  common.npm(
    [
      'find-dupes' // I actually found a use for this command!
    ],
    EXEC_OPTS,
    function (err, code, stdout, stderr) {
      t.ifError(err, 'successful dry run against fake install')
      t.notOk(code, 'npm ran without issue')
      t.notOk(stderr, 'npm printed no errors')
      t.same(
        stdout.trim().split('\n').map(ltrimm),
        [prolog].concat(body).map(ltrimm),
        'got expected output'
      )

      t.end()
    }
  )
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function setup (cb) {
  cleanup()

  mkdirp.sync(pkg)
  fs.writeFileSync(
    join(pkg, 'package.json'),
    JSON.stringify(deduper, null, 2)
  )

  mkdirp.sync(join(modules, 'first'))
  fs.writeFileSync(
    join(modules, 'first', 'package.json'),
    JSON.stringify(first, null, 2)
  )

  mkdirp.sync(join(modules, 'first', 'node_modules', 'firstUnique'))
  fs.writeFileSync(
    join(modules, 'first', 'node_modules', 'firstUnique', 'package.json'),
    JSON.stringify(firstUnique, null, 2)
  )

  mkdirp.sync(join(modules, 'first', 'node_modules', '@scope', 'shared'))
  fs.writeFileSync(
    join(modules, 'first', 'node_modules', '@scope', 'shared', 'package.json'),
    JSON.stringify(shared, null, 2)
  )

  mkdirp.sync(join(modules, 'second'))
  fs.writeFileSync(
    join(modules, 'second', 'package.json'),
    JSON.stringify(second, null, 2)
  )

  mkdirp.sync(join(modules, 'second', 'node_modules', 'secondUnique'))
  fs.writeFileSync(
    join(modules, 'second', 'node_modules', 'secondUnique', 'package.json'),
    JSON.stringify(secondUnique, null, 2)
  )

  mkdirp.sync(join(modules, 'second', 'node_modules', '@scope', 'shared'))
  fs.writeFileSync(
    join(modules, 'second', 'node_modules', '@scope', 'shared', 'package.json'),
    JSON.stringify(shared, null, 2)
  )
}

function cleanup () {
  rimraf.sync(pkg)
}
