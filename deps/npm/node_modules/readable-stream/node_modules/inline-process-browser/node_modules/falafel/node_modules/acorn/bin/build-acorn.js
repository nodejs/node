var fs = require("fs"), path = require("path")
var stream = require("stream")

var browserify = require("browserify")
var babelify = require("babelify").configure({loose: "all"})

process.chdir(path.resolve(__dirname, ".."))

browserify({standalone: "acorn"})
  .plugin(require('browserify-derequire'))
  .transform(babelify)
  .require("./src/index.js", {entry: true})
  .bundle()
  .on("error", function (err) { console.log("Error: " + err.message) })
  .pipe(fs.createWriteStream("dist/acorn.js"))

function acornShim(file) {
  var tr = new stream.Transform
  if (file == path.resolve(__dirname, "../src/index.js")) {
    var sent = false
    tr._transform = function(chunk, _, callback) {
      if (!sent) {
        sent = true
        callback(null, "module.exports = typeof acorn != 'undefined' ? acorn : _dereq_(\"./acorn\")")
      } else {
        callback()
      }
    }
  } else {
    tr._transform = function(chunk, _, callback) { callback(null, chunk) }
  }
  return tr
}

browserify({standalone: "acorn.loose"})
  .plugin(require('browserify-derequire'))
  .transform(acornShim)
  .transform(babelify)
  .require("./src/loose/index.js", {entry: true})
  .bundle()
  .on("error", function (err) { console.log("Error: " + err.message) })
  .pipe(fs.createWriteStream("dist/acorn_loose.js"))

browserify({standalone: "acorn.walk"})
  .plugin(require('browserify-derequire'))
  .transform(acornShim)
  .transform(babelify)
  .require("./src/walk/index.js", {entry: true})
  .bundle()
  .on("error", function (err) { console.log("Error: " + err.message) })
  .pipe(fs.createWriteStream("dist/walk.js"))
