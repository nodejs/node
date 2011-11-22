module.exports = which
which.sync = whichSync

var path = require("path")
  , fs
  , COLON = process.platform === "win32" ? ";" : ":"

try {
  fs = require("graceful-fs")
} catch (ex) {
  fs = require("fs")
}

// console.log(process.execPath)
// console.log(process.argv)

function isExe (mod, uid, gid) {
  //console.error("isExe?", (mod & 0111).toString(8))
  var ret = (mod & 0001)
      || (mod & 0010) && process.getgid && gid === process.getgid()
      || (mod & 0100) && process.getuid && uid === process.getuid()
  //console.error("isExe?", ret)
  return ret
}
function which (cmd, cb) {
  if (cmd.charAt(0) === "/") return cb(null, cmd)
  var pathEnv = (process.env.PATH || "").split(COLON)
    , pathExt = [""]
  if (process.platform === "win32") {
    pathEnv.push(process.cwd())
    pathExt = (process.env.PATHEXT || ".EXE").split(COLON)
  }
  //console.error("pathEnv", pathEnv)
  ;(function F (i, l) {
    if (i === l) return cb(new Error("not found: "+cmd))
    var p = path.resolve(pathEnv[i], cmd)
    ;(function E (ii, ll) {
      if (ii === ll) return F(i + 1, l)
      var ext = pathExt[ii]
      //console.error(p + ext)
      fs.stat(p + ext, function (er, stat) {
        if (!er &&
            stat &&
            stat.isFile() &&
            isExe(stat.mode, stat.uid, stat.gid)) {
          //console.error("yes, exe!", p + ext)
          return cb(null, p + ext)
        }
        return E(ii + 1, ll)
      })
    })(0, pathExt.length)
  })(0, pathEnv.length)
}


function whichSync (cmd) {
  if (cmd.charAt(0) === "/") return cmd
  var pathEnv = (process.env.PATH || "").split(COLON)
  for (var i = 0, l = pathEnv.length; i < l; i ++) {
    var p = path.join(pathEnv[i], cmd)
    if (p === process.execPath) return p
    var stat
    try { stat = fs.statSync(p) } catch (ex) {}
    if (stat && isExe(stat.mode, stat.uid, stat.gid)) return p
  }
  throw new Error("not found: "+cmd)
}
