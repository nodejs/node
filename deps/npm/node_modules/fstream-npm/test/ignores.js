var fs = require('graceful-fs')
var join = require('path').join

var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var test = require('tap').test

var Packer = require('..')

var pkg = join(__dirname, 'test-package')

var gitDir = join(pkg, '.git')

var elfJS = function () {/*
module.exports = function () {
  console.log("i'm a elf")
}
*/}.toString().split('\n').slice(1, -1).join()

var json = {
  'name': 'test-package',
  'version': '3.1.4',
  'main': 'elf.js'
}

test('setup', function (t) {
  setup()
  t.end()
})

var included = [
  'package.json',
  'elf.js'
]

test('follows npm package ignoring rules', function (t) {
  var subject = new Packer({ path: pkg, type: 'Directory', isDirectory: true })
  subject.on('entry', function (entry) {
    t.equal(entry.type, 'File', 'only files in this package')
    var filename = entry.basename
    t.ok(
      included.indexOf(filename) > -1,
      filename + ' is included'
    )
  })
  // need to do this so fstream doesn't explode when files are removed from
  // under it
  subject.on('end', function () { t.end() })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function setup () {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(
    join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )

  fs.writeFileSync(
    join(pkg, 'elf.js'),
    elfJS
  )

  fs.writeFileSync(
    join(pkg, '.npmrc'),
    'packaged=false'
  )

  var build = join(pkg, 'build')
  mkdirp.sync(build)
  fs.writeFileSync(
    join(build, 'config.gypi'),
    "i_wont_be_included_by_fstream='with any luck'"
  )

  fs.writeFileSync(
    join(build, 'npm-debug.log'),
    '0 lol\n'
  )

  mkdirp.sync(gitDir)
  fs.writeFileSync(
    join(gitDir, 'gitstub'),
    "won't fool git, also won't be included by fstream"
  )
}

function cleanup () {
  rimraf.sync(pkg)
}
