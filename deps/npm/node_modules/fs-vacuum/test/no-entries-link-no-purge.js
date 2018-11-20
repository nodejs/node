var path = require("path")

var test        = require("tap").test
var statSync    = require("graceful-fs").statSync
var symlinkSync = require("graceful-fs").symlinkSync
var readdirSync = require("graceful-fs").readdirSync
var mkdtemp     = require("tmp").dir
var mkdirp      = require("mkdirp")

var vacuum = require("../vacuum.js")

// CONSTANTS
var TEMP_OPTIONS = {
  unsafeCleanup : true,
  mode          : "0700"
}
var SHORT_PATH   = path.join("i", "am", "a", "path")
var TARGET_PATH  = path.join("target-link", "in", "the", "middle")
var PARTIAL_PATH = path.join(SHORT_PATH, "with", "a")
var FULL_PATH    = path.join(PARTIAL_PATH, "link")
var EXPANDO_PATH = path.join(SHORT_PATH, "with", "a", "link", "in", "the", "middle")

var messages = []
function log() { messages.push(Array.prototype.slice.call(arguments).join(" ")) }

var testBase, targetPath, partialPath, fullPath, expandoPath
test("xXx setup xXx", function (t) {
  mkdtemp(TEMP_OPTIONS, function (er, tmpdir) {
    t.ifError(er, "temp directory exists")

    testBase    = path.resolve(tmpdir, SHORT_PATH)
    targetPath  = path.resolve(tmpdir, TARGET_PATH)
    partialPath = path.resolve(tmpdir, PARTIAL_PATH)
    fullPath    = path.resolve(tmpdir, FULL_PATH)
    expandoPath = path.resolve(tmpdir, EXPANDO_PATH)

    mkdirp(partialPath, function (er) {
      t.ifError(er, "made test path")

      mkdirp(targetPath, function (er) {
        t.ifError(er, "made target path")

        symlinkSync(path.join(tmpdir, "target-link"), fullPath)

        t.end()
      })
    })
  })
})

test("remove up to a point", function (t) {
  vacuum(expandoPath, {purge : false, base : testBase, log : log}, function (er) {
    t.ifError(er, "cleaned up to base")

    t.equal(messages.length, 7, "got 6 removal & 1 finish message")
    t.equal(messages[6], "finished vacuuming up to " + testBase)

    var stat
    var verifyPath = expandoPath
    function verify() { stat = statSync(verifyPath) }

    for (var i = 0; i < 6; i++) {
      t.throws(verify, verifyPath + " cannot be statted")
      t.notOk(stat && stat.isDirectory(), verifyPath + " is totally gone")
      verifyPath = path.dirname(verifyPath)
    }

    t.doesNotThrow(function () {
      stat = statSync(testBase)
    }, testBase + " can be statted")
    t.ok(stat && stat.isDirectory(), testBase + " is still a directory")

    var files = readdirSync(testBase)
    t.equal(files.length, 0, "nothing left in base directory")

    t.end()
  })
})
