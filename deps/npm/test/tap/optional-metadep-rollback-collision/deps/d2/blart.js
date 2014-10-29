var rando = require("crypto").randomBytes
var resolve = require("path").resolve

var mkdirp = require("mkdirp")
var rimraf = require("rimraf")
var writeFile = require("graceful-fs").writeFile

var BASEDIR = resolve(__dirname, "arena")

var keepItGoingLouder = {}
var writers = 0
var errors = 0

function gensym() { return rando(16).toString("hex") }

function writeAlmostForever(filename) {
  if (!keepItGoingLouder[filename]) {
    writers--
    if (writers < 1) return done()
  }
  else {
    writeFile(filename, keepItGoingLouder[filename], function (err) {
      if (err) errors++

      writeAlmostForever(filename)
    })
  }
}

function done() {
  rimraf(BASEDIR, function () {
    if (errors > 0) console.log("not ok - %d errors", errors)
    else console.log("ok")
  })
}

mkdirp(BASEDIR, function go() {
  for (var i = 0; i < 16; i++) {
    var filename = resolve(BASEDIR, gensym() + ".txt")

    keepItGoingLouder[filename] = ""
    for (var j = 0; j < 512; j++) keepItGoingLouder[filename] += filename

    writers++
    writeAlmostForever(filename)
  }

  setTimeout(function viktor() {
    // kill all the writers
    keepItGoingLouder = {}
  }, 3 * 1000)
})
