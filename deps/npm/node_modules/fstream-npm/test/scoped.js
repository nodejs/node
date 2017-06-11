var fs = require('graceful-fs')
var join = require('path').join

var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var test = require('tap').test

var Packer = require('..')

var pkg = join(__dirname, 'test-package-scoped')

var elfJS = function () {/*
module.exports = function () {
  console.log("i'm a elf")
}
*/}.toString().split('\n').slice(1, -1).join()

var json = {
  'name': 'test-package-scoped',
  'version': '3.1.4',
  'main': 'elf.js',
  'bundledDependencies': [
    '@npmwombat/scoped'
  ]
}

test('setup', function (t) {
  setup()
  t.end()
})

var expected = [
  'package.json',
  'elf.js',
  join('node_modules', '@npmwombat', 'scoped', 'index.js'),
  join('node_modules', '@npmwombat', 'scoped', 'node_modules', 'example', 'index.js')
]

test('includes bundledDependencies', function (t) {
  var subject = new Packer({ path: pkg, type: 'Directory', isDirectory: true })
  var actual = []
  subject.on('entry', function (entry) {
    t.equal(entry.type, 'File', 'only files in this package')
    // include relative path in filename
    var filename = entry._path.slice(entry.root._path.length + 1)
    actual.push(filename)
  })
  // need to do this so fstream doesn't explode when files are removed from
  // under it
  subject.on('end', function () {
    // ensure we get *exactly* the results we expect by comparing in both
    // directions
    actual.forEach(function (filename) {
      t.ok(
        expected.indexOf(filename) > -1,
        filename + ' is included'
      )
    })
    expected.forEach(function (filename) {
      t.ok(
        actual.indexOf(filename) > -1,
        filename + ' is not included'
      )
    })
    t.end()
  })
})

test('cleanup', function (t) {
  // rimraf.sync chokes here for some reason
  rimraf(pkg, function () { t.end() })
})

function setup () {
  rimraf.sync(pkg)
  mkdirp.sync(pkg)
  fs.writeFileSync(
    join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )

  fs.writeFileSync(
    join(pkg, 'elf.js'),
    elfJS
  )

  var scopedDir = join(pkg, 'node_modules', '@npmwombat', 'scoped')
  mkdirp.sync(scopedDir)
  fs.writeFileSync(
    join(scopedDir, 'index.js'),
    "console.log('hello wombat')"
  )
  var scopedContent = join(scopedDir, 'node_modules', 'example')
  mkdirp.sync(scopedContent)
  fs.writeFileSync(
    join(scopedContent, 'index.js'),
    "console.log('hello example')"
  )
}
