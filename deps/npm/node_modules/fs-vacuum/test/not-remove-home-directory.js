var path = require('path')

var test = require('tap').test
var mkdtemp = require('tmp').dir
var mkdirp = require('mkdirp')

var vacuum = require('../vacuum.js')

// CONSTANTS
var TEMP_OPTIONS = {
  unsafeCleanup: true,
  mode: '0700'
}

var BASE_PATH = path.join('foo')
var HOME_PATH = path.join(BASE_PATH, 'foo', 'bar')

var messages = []
function log () { messages.push(Array.prototype.slice.call(arguments).join(' ')) }

var homePath, basePath, realHome
test('xXx setup xXx', function (t) {
  mkdtemp(TEMP_OPTIONS, function (er, tmpdir) {
    t.ifError(er, 'temp directory exists')

    homePath = path.resolve(tmpdir, HOME_PATH)
    basePath = path.resolve(tmpdir, BASE_PATH)

    realHome = process.env.HOME
    process.env.HOME = homePath

    mkdirp(homePath, function (er) {
      t.ifError(er, 'made test path')
      t.end()
    })
  })
})

test('do not remove home directory', function (t) {
  vacuum(homePath, {purge: false, base: basePath, log: log}, function (er) {
    t.ifError(er, 'cleaned up to base')
    t.equal(messages[0], 'quitting because cannot remove home directory ' + homePath)
    process.env.HOME = realHome
    t.end()
  })
})
