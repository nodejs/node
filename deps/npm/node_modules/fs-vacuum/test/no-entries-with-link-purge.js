var path = require('path')

var test = require('tap').test
var statSync = require('graceful-fs').statSync
var writeFileSync = require('graceful-fs').writeFileSync
var symlinkSync = require('graceful-fs').symlinkSync
var mkdtemp = require('tmp').dir
var mkdirp = require('mkdirp')

var vacuum = require('../vacuum.js')

// CONSTANTS
var TEMP_OPTIONS = {
  unsafeCleanup: true,
  mode: '0700'
}
var SHORT_PATH = path.join('i', 'am', 'a', 'path')
var TARGET_PATH = 'link-target'
var FIRST_FILE = path.join(TARGET_PATH, 'monsieurs')
var SECOND_FILE = path.join(TARGET_PATH, 'mesdames')
var PARTIAL_PATH = path.join(SHORT_PATH, 'with', 'a', 'definite')
var FULL_PATH = path.join(PARTIAL_PATH, 'target')

var messages = []
function log () { messages.push(Array.prototype.slice.call(arguments).join(' ')) }

var testBase, partialPath, fullPath, targetPath
test('xXx setup xXx', function (t) {
  mkdtemp(TEMP_OPTIONS, function (er, tmpdir) {
    t.ifError(er, 'temp directory exists')

    testBase = path.resolve(tmpdir, SHORT_PATH)
    targetPath = path.resolve(tmpdir, TARGET_PATH)
    partialPath = path.resolve(tmpdir, PARTIAL_PATH)
    fullPath = path.resolve(tmpdir, FULL_PATH)

    mkdirp(partialPath, function (er) {
      t.ifError(er, 'made test path')

      mkdirp(targetPath, function (er) {
        t.ifError(er, 'made target path')

        writeFileSync(path.resolve(tmpdir, FIRST_FILE), new Buffer("c'est vraiment joli"))
        writeFileSync(path.resolve(tmpdir, SECOND_FILE), new Buffer('oui oui'))
        symlinkSync(targetPath, fullPath)

        t.end()
      })
    })
  })
})

test('remove up to a point', function (t) {
  vacuum(fullPath, {purge: true, base: testBase, log: log}, function (er) {
    t.ifError(er, 'cleaned up to base')

    t.equal(messages.length, 5, 'got 4 removal & 1 finish message')
    t.equal(messages[0], 'purging ' + fullPath)
    t.equal(messages[4], 'finished vacuuming up to ' + testBase)

    var stat
    var verifyPath = fullPath
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
