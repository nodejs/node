'use strict'
var test = require('tap').test
var common = require('../common-tap.js')
var path = require('path')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var basepath = path.resolve(__dirname, path.basename(__filename, '.js'))
var fixturepath = path.resolve(basepath, 'npm-test-url-dep')
var modulepath = path.resolve(basepath, 'node_modules')
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir

var fixture = new Tacks(
  Dir({
    README: File(
      'just an npm test\n'
    ),
    'package.json': File({
      name: 'npm-test-url-dep',
      version: '1.2.3',
      dependencies: {
        jsonify: 'https://github.com/substack/jsonify/tarball/master',
        sax: 'isaacs/sax-js',
        'canonical-host': 'git://github.com/isaacs/canonical-host'
      }
    })
  })
)

test('setup', function (t) {
  setup()
  t.done()
})

test('url-dep', function (t) {
  common.npm(['install', fixturepath], {cwd: basepath}, installCheckAndTest)
  function installCheckAndTest (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'install went ok')
    t.done()
  }
})

test('cleanup', function (t) {
  cleanup()
  t.done()
})

function setup () {
  cleanup()
  fixture.create(fixturepath)
  mkdirp.sync(modulepath)
}

function cleanup () {
  fixture.remove(fixturepath)
  rimraf.sync(basepath)
}
