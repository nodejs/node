module.exports = isexe
isexe.sync = sync

var fs = require('fs')

function checkPathExt (path, options) {
  var pathext = options.pathExt !== undefined ?
    options.pathExt : process.env.PATHEXT

  if (!pathext) {
    return true
  }

  pathext = pathext.split(';')
  if (pathext.indexOf('') !== -1) {
    return true
  }
  for (var i = 0; i < pathext.length; i++) {
    var p = pathext[i].toLowerCase()
    if (p && path.substr(-p.length).toLowerCase() === p) {
      return true
    }
  }
  return false
}

function isexe (path, options, cb) {
  fs.stat(path, function (er, st) {
    cb(er, er ? false : checkPathExt(path, options))
  })
}

function sync (path, options) {
  fs.statSync(path)
  return checkPathExt(path, options)
}
