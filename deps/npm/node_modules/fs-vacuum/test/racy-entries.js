var path = require('path')

var test = require('tap').test

var readdir = require('graceful-fs').readdir
var readdirSync = require('graceful-fs').readdirSync
var statSync = require('graceful-fs').statSync
var writeFile = require('graceful-fs').writeFile
var mkdirp = require('mkdirp')
var mkdtemp = require('tmp').dir
var tmpFile = require('tmp').file

var requireInject = require('require-inject')
var vacuum = requireInject('../vacuum.js', {
  'graceful-fs': {
    'lstat': require('graceful-fs').lstat,
    'readdir': function (dir, cb) {
      readdir(dir, function (err, files) {
        // Simulate racy removal by creating a file after vacuum
        // thinks the directory is empty
        if (dir === partialPath && files.length === 0) {
          tmpFile({dir: dir}, function (err, path, fd) {
            if (err) throw err
            cb(err, files)
          })
        } else {
          cb(err, files)
        }
      })
    },
    'rmdir': require('graceful-fs').rmdir,
    'unlink': require('graceful-fs').unlink
  }
})

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

test('racy removal should quit gracefully', function (t) {
  vacuum(fullPath, {purge: false, base: testBase, log: log}, function (er) {
    t.ifError(er, 'cleaned up to base')

    t.equal(messages.length, 3, 'got 2 removal & 1 quit message')
    t.equal(messages[2], 'quitting because new (racy) entries in ' + partialPath)

    var stat
    var verifyPath = fullPath

    function verify () { stat = statSync(verifyPath) }

    // handle the file separately
    t.throws(verify, verifyPath + ' cannot be statted')
    t.notOk(stat && stat.isFile(), verifyPath + ' is totally gone')
    verifyPath = path.dirname(verifyPath)

    for (var i = 0; i < 4; i++) {
      t.doesNotThrow(function () {
        stat = statSync(verifyPath)
      }, verifyPath + ' can be statted')
      t.ok(stat && stat.isDirectory(), verifyPath + ' is still a directory')
      verifyPath = path.dirname(verifyPath)
    }

    t.doesNotThrow(function () {
      stat = statSync(testBase)
    }, testBase + ' can be statted')
    t.ok(stat && stat.isDirectory(), testBase + ' is still a directory')

    var files = readdirSync(testBase)
    t.equal(files.length, 1, 'base directory untouched')

    t.end()
  })
})
