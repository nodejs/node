module.exports = fileCompletion

var find = require("../find.js")
  , mkdir = require("../mkdir-p.js")
  , path = require("path")

function fileCompletion (root, req, depth, cb) {
  if (typeof cb !== "function") cb = depth, depth = Infinity
  mkdir(root, function (er) {
    if (er) return cb(er)
    function dirFilter (f, type) {
      // return anything that is a file,
      // or not exactly the req.
      return type !== "dir" ||
        ( f && f !== path.join(root, req)
         && f !== path.join(root, req) + "/" )
    }
    find(path.join(root, req), dirFilter, depth, function (er, files) {
      if (er) return cb(er)
      return cb(null, (files || []).map(function (f) {
        return path.join(req, f.substr(root.length + 1)
                               .substr((f === req ? path.dirname(req)
                                                  : req).length)
                               .replace(/^\//, ""))
      }))
    })
  })
}

