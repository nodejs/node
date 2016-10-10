module.exports = isexe
isexe.sync = sync

var fs = require('fs')

function isexe (path, _, cb) {
  fs.access(path, fs.X_OK, function (er) {
    cb(er, !er)
  })
}

function sync (path, _) {
  fs.accessSync(path, fs.X_OK)
  return true
}
