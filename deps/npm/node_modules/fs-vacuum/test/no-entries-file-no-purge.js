var path = require('path')

var test = require('tap').test
var statSync = require('graceful-fs').statSync
var writeFile = require('graceful-fs').writeFile
var readdirSync = require('graceful-fs').readdirSync
var mkdtemp = require('tmp').dir
var mkdirp = require('mkdirp')

var vacuum = require('../vacuum.js')

// CONSTANTS
var TEMP_OPTIONS = {
  unsafeCleanup: true,
  mode: '0700'
}
var SHORT_PATH = path.join('i', 'am', 'a', 'path')
var PARTIAL_PATH = path.join(SHORT_PATH, 'that', 'ends', 'at', 'a')
var FULL_PATH = path.join(PARTIAL_PATH, 'file')

var messages = []
function log () { messages.push(Array.prototype.slice.call(arguments).join(' ')) }

var testBase, partialPath, fullPath
test('xXx setup xXx', function (t) {
  mkdtemp(TEMP_OPTIONS, function (er, tmpdir) {
    t.ifError(er, 'temp directory exists')

    testBase = path.resolve(tmpdir, SHORT_PATH)
    partialPath = path.resolve(tmpdir, PARTIAL_PATH)
    fullPath = path.resolve(tmpdir, FULL_PATH)

    mkdirp(partialPath, function (er) {
      t.ifError(er, 'made test path')

      writeFile(fullPath, new Buffer('hi'), function (er) {
        t.ifError(er, 'made file')

        t.end()
      })
    })
  })
})

test('remove up to a point', function (t) {
  vacuum(fullPath, {purge: false, base: testBase, log: log}, function (er) {
    t.ifError(er, 'cleaned up to base')

    t.equal(messages.length, 6, 'got 5 removal & 1 finish message')
    t.equal(messages[5], 'finished vacuuming up to ' + testBase)

    var stat
    var verifyPath = fullPath

    function verify () { stat = statSync(verifyPath) }

    // handle the file separately
    t.throws(verify, verifyPath + ' cannot be statted')
    t.notOk(stat && stat.isFile(), verifyPath + ' is totally gone')
    verifyPath = path.dirname(verifyPath)

    for (var i = 0; i < 4; i++) {
      t.throws(verify, verifyPath + ' cannot be statted')
      t.notOk(stat && stat.isDirectory(), verifyPath + ' is totally gone')
      verifyPath = path.dirname(verifyPath)
    }

    t.doesNotThrow(function () {
      stat = statSync(testBase)
    }, testBase + ' can be statted')
    t.ok(stat && stat.isDirectory(), testBase + ' is still a directory')

    var files = readdirSync(testBase)
    t.equal(files.length, 0, 'nothing left in base directory')

    t.end()
  })
})
