module.exports = which
which.sync = whichSync

var isWindows = process.platform === 'win32' ||
    process.env.OSTYPE === 'cygwin' ||
    process.env.OSTYPE === 'msys'

var path = require('path')
var COLON = isWindows ? ';' : ':'
var isExe
var fs = require('fs')
var isAbsolute = require('is-absolute')

if (isWindows) {
  // On windows, there is no good way to check that a file is executable
  isExe = function isExe () { return true }
} else {
  isExe = function isExe (mod, uid, gid) {
    var ret = (mod & 0001)
        || (mod & 0010) && process.getgid && gid === process.getgid()
        || (mod & 0100) && process.getuid && uid === process.getuid()
        || (mod & 0110) && process.getuid && 0   === process.getuid()

    if (process.getgroups && (mod & 0010)) {
      var groups = process.getgroups()
      for (var g = 0; g < groups.length; g++) {
        if (groups[g] === gid)
          return true
      }
    }

    return ret
  }
}

function which (cmd, opt, cb) {
  if (typeof opt === 'function') {
    cb = opt
    opt = {}
  }

  var colon = opt.colon || COLON
  var pathEnv = opt.path || process.env.PATH || ''
  var pathExt = ['']

  // On windows, env.Path is common.
  if (isWindows && !pathEnv) {
    var k = Object.keys(process.env)
    for (var p = 0; p < k.length; p++) {
      if (p.toLowerCase() === 'path') {
        pathEnv = process.env[p]
        break
      }
    }
  }

  pathEnv = pathEnv.split(colon)

  if (isWindows) {
    pathEnv.unshift(process.cwd())
    pathExt = (opt.pathExt || process.env.PATHEXT || '.EXE').split(colon)
    if (cmd.indexOf('.') !== -1 && pathExt[0] !== '')
      pathExt.unshift('')
  }

  // If it's absolute, then we don't bother searching the pathenv.
  // just check the file itself, and that's it.
  if (isAbsolute(cmd))
    pathEnv = ['']

  ;(function F (i, l) {
    if (i === l) return cb(new Error('not found: '+cmd))
    var p = path.resolve(pathEnv[i], cmd)
    ;(function E (ii, ll) {
      if (ii === ll) return F(i + 1, l)
      var ext = pathExt[ii]
      fs.stat(p + ext, function (er, stat) {
        if (!er &&
            stat.isFile() &&
            isExe(stat.mode, stat.uid, stat.gid)) {
          return cb(null, p + ext)
        }
        return E(ii + 1, ll)
      })
    })(0, pathExt.length)
  })(0, pathEnv.length)
}

function whichSync (cmd, opt) {
  if (!opt)
    opt = {}

  var colon = opt.colon || COLON

  var pathEnv = opt.path || process.env.PATH || ''
  var pathExt = ['']

  // On windows, env.Path is common.
  if (isWindows && !pathEnv) {
    var k = Object.keys(process.env)
    for (var p = 0; p < k.length; p++) {
      if (p.toLowerCase() === 'path') {
        pathEnv = process.env[p]
        break
      }
    }
  }

  pathEnv = pathEnv.split(colon)

  if (isWindows) {
    pathEnv.unshift(process.cwd())
    pathExt = (opt.pathExt || process.env.PATHEXT || '.EXE').split(colon)
    if (cmd.indexOf('.') !== -1 && pathExt[0] !== '')
      pathExt.unshift('')
  }

  // If it's absolute, then we don't bother searching the pathenv.
  // just check the file itself, and that's it.
  if (isAbsolute(cmd))
    pathEnv = ['']

  for (var i = 0, l = pathEnv.length; i < l; i ++) {
    var p = path.join(pathEnv[i], cmd)
    for (var j = 0, ll = pathExt.length; j < ll; j ++) {
      var cur = p + pathExt[j]
      var stat
      try {
        stat = fs.statSync(cur)
        if (stat.isFile() && isExe(stat.mode, stat.uid, stat.gid))
          return cur
      } catch (ex) {}
    }
  }

  throw new Error('not found: '+cmd)
}
