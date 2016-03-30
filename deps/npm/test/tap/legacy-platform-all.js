'use strict'
var test = require('tap').test
var common = require('../common-tap.js')
var path = require('path')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var basepath = path.resolve(__dirname, path.basename(__filename, '.js'))
var fixturepath = path.resolve(basepath, 'npm-test-platform-all')
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
      name: 'npm-test-platform-all',
      version: '9.9.9-9',
      homepage: 'http://www.zombo.com/',
      os: [
        'darwin',
        'linux',
        'win32',
        'solaris',
        'haiku',
        'sunos',
        'freebsd',
        'openbsd',
        'netbsd'
      ],
      cpu: [
        'arm',
        'mips',
        'ia32',
        'x64',
        'sparc'
      ]
    })
  })
)

test('setup', function (t) {
  setup()
  t.done()
})

test('platform-all', function (t) {
  common.npm(['install', fixturepath], {cwd: basepath}, installCheckAndTest)
  function installCheckAndTest (err, code, stdout, stderr) {
    if (err) throw err
    t.is(stderr, '', 'no error messages')
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
