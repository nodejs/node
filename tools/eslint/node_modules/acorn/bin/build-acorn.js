var fs = require("fs"), path = require("path")
var stream = require("stream")

var browserify = require("browserify")
var babel = require('babel-core')
var babelify = require("babelify").configure({loose: "all"})

process.chdir(path.resolve(__dirname, ".."))

browserify({standalone: "acorn"})
  .plugin(require('browserify-derequire'))
  .transform(babelify)
  .require("./src/index.js", {entry: true})
  .bundle()
  .on("error", function (err) { console.log("Error: " + err.message) })
  .pipe(fs.createWriteStream("dist/acorn.js"))

var ACORN_PLACEHOLDER = "this_function_call_should_be_replaced_with_a_call_to_load_acorn()";
function acornShimPrepare(file) {
  var tr = new stream.Transform
  if (file == path.resolve(__dirname, "../src/index.js")) {
    var sent = false
    tr._transform = function(chunk, _, callback) {
      if (!sent) {
        sent = true
        callback(null, ACORN_PLACEHOLDER);
      } else {
        callback()
      }
    }
  } else {
    tr._transform = function(chunk, _, callback) { callback(null, chunk) }
  }
  return tr
}
function acornShimComplete() {
  var tr = new stream.Transform
  var buffer = "";
  tr._transform = function(chunk, _, callback) {
    buffer += chunk.toString("utf8");
    callback();
  };
  tr._flush = function (callback) {
    tr.push(buffer.replace(ACORN_PLACEHOLDER, "module.exports = typeof acorn != 'undefined' ? acorn : require(\"./acorn\")"));
    callback(null);
  };
  return tr;
}

browserify({standalone: "acorn.loose"})
  .plugin(require('browserify-derequire'))
  .transform(acornShimPrepare)
  .transform(babelify)
  .require("./src/loose/index.js", {entry: true})
  .bundle()
  .on("error", function (err) { console.log("Error: " + err.message) })
  .pipe(acornShimComplete())
  .pipe(fs.createWriteStream("dist/acorn_loose.js"))

browserify({standalone: "acorn.walk"})
  .plugin(require('browserify-derequire'))
  .transform(acornShimPrepare)
  .transform(babelify)
  .require("./src/walk/index.js", {entry: true})
  .bundle()
  .on("error", function (err) { console.log("Error: " + err.message) })
  .pipe(acornShimComplete())
  .pipe(fs.createWriteStream("dist/walk.js"))

babel.transformFile("./src/bin/acorn.js", function (err, result) {
  if (err) return console.log("Error: " + err.message)
  fs.writeFile("bin/acorn", result.code, function (err) {
    if (err) return console.log("Error: " + err.message)

    // Make bin/acorn executable
    if (process.platform === 'win32')
      return
    var stat = fs.statSync("bin/acorn")
    var newPerm = stat.mode | parseInt('111', 8)
    fs.chmodSync("bin/acorn", newPerm)
  })
})
