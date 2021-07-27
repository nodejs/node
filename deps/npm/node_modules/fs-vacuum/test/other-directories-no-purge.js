var path = require('path')

var test = require('tap').test
var statSync = require('graceful-fs').statSync
var mkdtemp = require('tmp').dir
var mkdirp = require('mkdirp')

var vacuum = require('../vacuum.js')

// CONSTANTS
var TEMP_OPTIONS = {
  unsafeCleanup: true,
  mode: '0700'
}
var SHORT_PATH = path.join('i', 'am', 'a', 'path')
var REMOVE_PATH = path.join(SHORT_PATH, 'of', 'a', 'certain', 'length')
var OTHER_PATH = path.join(SHORT_PATH, 'of', 'no', 'qualities')

var messages = []
function log () { messages.push(Array.prototype.slice.call(arguments).join(' ')) }

var testBase, testPath, otherPath
test('xXx setup xXx', function (t) {
  mkdtemp(TEMP_OPTIONS, function (er, tmpdir) {
    t.ifError(er, 'temp directory exists')

    testBase = path.resolve(tmpdir, SHORT_PATH)
    testPath = path.resolve(tmpdir, REMOVE_PATH)
    otherPath = path.resolve(tmpdir, OTHER_PATH)

    mkdirp(testPath, function (er) {
      t.ifError(er, 'made test path')

      mkdirp(otherPath, function (er) {
        t.ifError(er, 'made other path')

        t.end()
      })
    })
  })
})

test('remove up to a point', function (t) {
  vacuum(testPath, {purge: false, base: testBase, log: log}, function (er) {
    t.ifError(er, 'cleaned up to base')

    t.equal(messages.length, 4, 'got 3 removal & 1 finish message')
    t.equal(
      messages[3], 'quitting because other entries in ' + testBase + '/of',
      'got expected final message'
    )

    var stat
    var verifyPath = testPath
    function verify () { stat = statSync(verifyPath) }

    for (var i = 0; i < 3; i++) {
      t.throws(verify, verifyPath + ' cannot be statted')
      t.notOk(stat && stat.isDirectory(), verifyPath + ' is totally gone')
      verifyPath = path.dirname(verifyPath)
    }

    t.doesNotThrow(function () {
      stat = statSync(otherPath)
    }, otherPath + ' can be statted')
    t.ok(stat && stat.isDirectory(), otherPath + ' is still a directory')

    var intersection = path.join(testBase, 'of')
    t.doesNotThrow(function () {
      stat = statSync(intersection)
    }, intersection + ' can be statted')
    t.ok(stat && stat.isDirectory(), intersection + ' is still a directory')

    t.end()
  })
})
