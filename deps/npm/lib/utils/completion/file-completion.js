module.exports = fileCompletion

var mkdir = require("mkdirp")
  , path = require("path")
  , glob = require("glob")

function fileCompletion (root, req, depth, cb) {
  if (typeof cb !== "function") cb = depth, depth = Infinity
  mkdir(root, function (er) {
    if (er) return cb(er)

    // can be either exactly the req, or a descendent
    var pattern = root + "/{" + req + "," + req + "/**/*}"
      , opts = { mark: true, dot: true, maxDepth: depth }
    glob(pattern, opts, function (er, files) {
      if (er) return cb(er)
      return cb(null, (files || []).map(function (f) {
        var tail = f.substr(root.length + 1).replace(/^\//, "")
        return path.join(req, tail)
      }))
    })
  })
}
