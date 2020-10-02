module.exports = fileCompletion

const mkdir = require('mkdirp')
const glob = require('glob')

function fileCompletion (root, req, depth, cb) {
  if (typeof cb !== 'function') {
    cb = depth
    depth = Infinity
  }
  mkdir(root).catch(cb).then(() => {
    // can be either exactly the req, or a descendent
    var pattern = root + '/{' + req + ',' + req + '/**/*}'
    var opts = { mark: true, dot: true, maxDepth: depth }
    glob(pattern, opts, function (er, files) {
      if (er) return cb(er)
      return cb(null, (files || []).map(function (f) {
        return f.substr(root.length + 1).replace(/^\/|\/$/g, '')
      }))
    })
  })
}
