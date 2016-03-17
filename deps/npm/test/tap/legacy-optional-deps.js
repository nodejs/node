'use strict'
var path = require('path')
var fs = require('fs')
var test = require('tap').test
var common = require('../common-tap.js')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var basepath = path.resolve(__dirname, path.basename(__filename, '.js'))
var fixturepath = path.resolve(basepath, 'npm-test-optional-deps')
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
      name: 'npm-test-optional-deps',
      version: '1.2.5',
      optionalDependencies: {
        'npm-test-foobarzaaakakaka': 'http://example.com/',
        async: '10.999.14234',
        mkdirp: '0.3.5',
        optimist: 'some invalid version 99 #! $$ x y z',
        'npm-test-failer': '*'
      }
    })
  })
)

var server

test('setup', function (t) {
  setup()
  mr({port: common.port}, function (err, s) {
    if (err) throw err
    server = s
    t.done()
  })
})

test('optional-deps', function (t) {
  server.get('/npm-test-failer').reply(404, {error: 'nope'})

  var opts = ['--registry=' + common.registry, '--timeout=100']
  common.npm(opts.concat(['install', fixturepath]), {cwd: basepath}, installCheckAndTest)

  function installCheckAndTest (err, code, stdout, stderr) {
    if (err) throw err
    if (stderr) console.error(stderr)
    server.done()
    t.is(code, 0, 'install went ok')
    var subpath = modulepath + '/npm-test-optional-deps/node_modules/'
    var dir = fs.readdirSync(subpath)
    t.isDeeply(dir, ['mkdirp'], 'only one optional dep should be there')
    t.is(require(path.resolve(subpath, 'mkdirp', 'package.json')).version, '0.3.5', 'mkdirp version right')
    t.done()
  }
})

test('cleanup', function (t) {
  cleanup()
  server.close()
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
