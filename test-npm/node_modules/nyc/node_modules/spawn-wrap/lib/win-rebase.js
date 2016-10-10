var re = /^\s*("*)([^"]*?\b(?:node|iojs)(?:\.exe)?)("*)( |$)/
var npmre = /^\s*("*)([^"]*?\b(?:npm))("*)( |$)/
var path_ = require('path')
if (path_.win32) path_ = path_.win32

module.exports = function (path, rebase, whichOrUndefined) {
  var m = path.match(re)
  if (!m) {
    m = path.match(npmre)
    if (!m) return path
    var npmPath = whichOrUndefined('npm') || 'npm'
    npmPath = path_.dirname(npmPath) + '\\node_modules\\npm\\bin\\npm-cli.js'
    return path.replace(npmre, m[1] + rebase + ' "' + npmPath + '"' + m[3] + m[4])
  }
  // preserve the quotes
  var replace = m[1] + rebase + m[3] + m[4]
  return path.replace(re, replace)
}
