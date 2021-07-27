// only remove the thing if it's a symlink into a specific folder. This is
// a very common use-case of npm's, but not so common elsewhere.

exports = module.exports = gentlyRm

var gentleFS = require('gentle-fs')
var gentleFSOpts = require('../config/gentle-fs.js')

function gentlyRm (target, gently, base, cb) {
  if (!cb) {
    cb = base
    base = undefined
  }

  if (!cb) {
    cb = gently
    gently = false
  }

  return gentleFS.rm(target, gentleFSOpts(gently, base), cb)
}
