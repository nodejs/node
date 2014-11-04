var spawn = require("child_process").spawn
var path = require("path")
var fs = require("fs")

var port = exports.port = 1337
exports.registry = "http://localhost:" + port
process.env.npm_config_loglevel = "error"

var npm_config_cache = path.resolve(__dirname, "npm_cache")
exports.npm_config_cache = npm_config_cache

var bin = exports.bin = require.resolve("../bin/npm-cli.js")
var once = require("once")

exports.npm = function (cmd, opts, cb) {
  cb = once(cb)
  cmd = [bin].concat(cmd)
  opts = opts || {}

  opts.env = opts.env ? opts.env : process.env
  if (!opts.env.npm_config_cache) {
    opts.env.npm_config_cache = npm_config_cache
  }

  var stdout = ""
    , stderr = ""
    , node = process.execPath
    , child = spawn(node, cmd, opts)

  if (child.stderr) child.stderr.on("data", function (chunk) {
    stderr += chunk
  })

  if (child.stdout) child.stdout.on("data", function (chunk) {
    stdout += chunk
  })

  child.on("error", cb)

  child.on("close", function (code) {
    cb(null, code, stdout, stderr)
  })
  return child
}

// based on http://bit.ly/1tkI6DJ
function deleteNpmCacheRecursivelySync(cache) {
  cache = cache ? cache : npm_config_cache
  var files = []
  var res
  if( fs.existsSync(cache) ) {
    files = fs.readdirSync(cache)
    files.forEach(function(file,index) {
      var curPath = path.resolve(cache, file)
      if(fs.lstatSync(curPath).isDirectory()) { // recurse
        deleteNpmCacheRecursivelySync(curPath)
      } else { // delete file
        if (res = fs.unlinkSync(curPath))
          throw Error("Failed to delete file " + curPath + ", error " + res)
      }
    })
    if (res = fs.rmdirSync(cache))
      throw Error("Failed to delete directory " + cache + ", error " + res)
  }
  return 0
}
exports.deleteNpmCacheRecursivelySync = deleteNpmCacheRecursivelySync