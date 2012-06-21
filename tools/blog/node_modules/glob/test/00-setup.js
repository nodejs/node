// just a little pre-run script to set up the fixtures.
// zz-finish cleans it up

var mkdirp = require("mkdirp")
var path = require("path")
var i = 0
var tap = require("tap")
var fs = require("fs")
var rimraf = require("rimraf")

var files =
[ "a/.abcdef/x/y/z/a"
, "a/abcdef/g/h"
, "a/abcfed/g/h"
, "a/b/c/d"
, "a/bc/e/f"
, "a/c/d/c/b"
, "a/cb/e/f"
]

var symlinkTo = path.resolve(__dirname, "a/symlink/a/b/c")
var symlinkFrom = "../.."

files = files.map(function (f) {
  return path.resolve(__dirname, f)
})

tap.test("remove fixtures", function (t) {
  rimraf(path.resolve(__dirname, "a"), function (er) {
    t.ifError(er, "remove fixtures")
    t.end()
  })
})

files.forEach(function (f) {
  tap.test(f, function (t) {
    var d = path.dirname(f)
    mkdirp(d, 0755, function (er) {
      if (er) {
        t.fail(er)
        return t.bailout()
      }
      fs.writeFile(f, "i like tests", function (er) {
        t.ifError(er, "make file")
        t.end()
      })
    })
  })
})

tap.test("symlinky", function (t) {
  var d = path.dirname(symlinkTo)
  console.error("mkdirp", d)
  mkdirp(d, 0755, function (er) {
    t.ifError(er)
    fs.symlink(symlinkFrom, symlinkTo, function (er) {
      t.ifError(er, "make symlink")
      t.end()
    })
  })
})
