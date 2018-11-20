// npm pack <pkg>
// Packs the specified package into a .tgz file, which can then
// be installed.

module.exports = pack

var npm = require("./npm.js")
  , install = require("./install.js")
  , cache = require("./cache.js")
  , fs = require("graceful-fs")
  , chain = require("slide").chain
  , path = require("path")
  , cwd = process.cwd()
  , writeStream = require('fs-write-stream-atomic')
  , cachedPackageRoot = require("./cache/cached-package-root.js")

pack.usage = "npm pack <pkg>"

// if it can be installed, it can be packed.
pack.completion = install.completion

function pack (args, silent, cb) {
  if (typeof cb !== "function") cb = silent, silent = false

  if (args.length === 0) args = ["."]

  chain(args.map(function (arg) { return function (cb) {
    pack_(arg, cb)
  }}), function (er, files) {
    if (er || silent) return cb(er, files)
    printFiles(files, cb)
  })
}

function printFiles (files, cb) {
  files = files.map(function (file) {
    return path.relative(cwd, file)
  })
  console.log(files.join("\n"))
  cb()
}

// add to cache, then cp to the cwd
function pack_ (pkg, cb) {
  cache.add(pkg, null, null, false, function (er, data) {
    if (er) return cb(er)

    // scoped packages get special treatment
    var name = data.name
    if (name[0] === "@") name = name.substr(1).replace(/\//g, "-")
    var fname = name + "-" + data.version + ".tgz"

    var cached = path.join(cachedPackageRoot(data), "package.tgz")
      , from = fs.createReadStream(cached)
      , to = writeStream(fname)
      , errState = null

    from.on("error", cb_)
    to.on("error", cb_)
    to.on("close", cb_)
    from.pipe(to)

    function cb_ (er) {
      if (errState) return
      if (er) return cb(errState = er)
      cb(null, fname)
    }
  })
}
