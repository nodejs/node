var path = require('path')

var test = require('tap').test
var statSync = require('graceful-fs').statSync
var writeFileSync = require('graceful-fs').writeFileSync
var mkdtemp = require('tmp').dir
var mkdirp = require('mkdirp')

var vacuum = require('../vacuum.js')

// CONSTANTS
var TEMP_OPTIONS = {
  unsafeCleanup: true,
  mode: '0700'
}
var SHORT_PATH = path.join('i', 'am', 'a', 'path')
var LONG_PATH = path.join(SHORT_PATH, 'of', 'a', 'certain', 'kind')
var FIRST_FILE = path.join(LONG_PATH, 'monsieurs')
var SECOND_FILE = path.join(LONG_PATH, 'mesdames')

var messages = []
function log () { messages.push(Array.prototype.slice.call(arguments).join(' ')) }

var testPath, testBase
test('xXx setup xXx', function (t) {
  mkdtemp(TEMP_OPTIONS, function (er, tmpdir) {
    t.ifError(er, 'temp directory exists')

    testBase = path.resolve(tmpdir, SHORT_PATH)
    testPath = path.resolve(tmpdir, LONG_PATH)

    mkdirp(testPath, function (er) {
      t.ifError(er, 'made test path')

      writeFileSync(path.resolve(tmpdir, FIRST_FILE), new Buffer("c'est vraiment joli"))
      writeFileSync(path.resolve(tmpdir, SECOND_FILE), new Buffer('oui oui'))
      t.end()
    })
  })
})

test('remove up to a point', function (t) {
  vacuum(testPath, {purge: true, base: testBase, log: log}, function (er) {
    t.ifError(er, 'cleaned up to base')

    t.equal(messages.length, 5, 'got 4 removal & 1 finish message')
    t.equal(messages[0], 'purging ' + testPath)
    t.equal(messages[4], 'finished vacuuming up to ' + testBase)

    var stat
    var verifyPath = testPath
    function verify () { stat = statSync(verifyPath) }

    for (var i = 0; i < 4; i++) {
      t.throws(verify, verifyPath + ' cannot be statted')
      t.notOk(stat && stat.isDirectory(), verifyPath + ' is totally gone')
      verifyPath = path.dirname(verifyPath)
    }

    t.doesNotThrow(function () {
      stat = statSync(testBase)
    }, testBase + ' can be statted')
    t.ok(stat && stat.isDirectory(), testBase + ' is still a directory')

    t.end()
  })
})
